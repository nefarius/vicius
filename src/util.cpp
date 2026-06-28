#include "pch.h"
#include "Util.h"
#include <Softpub.h>
#include <wintrust.h>
#include <Shlobj.h>
#include <Msi.h>
#include <tlhelp32.h>


EXTERN_C IMAGE_DOS_HEADER __ImageBase;


namespace util
{
    std::filesystem::path GetImageBasePathW()
    {
        wchar_t myPath[ MAX_PATH + 1 ] = {0};

        GetModuleFileNameW(reinterpret_cast<HINSTANCE>(&__ImageBase), myPath, MAX_PATH + 1);

        return std::wstring(myPath);
    }

    semver::version GetVersionFromFile(const std::filesystem::path& filePath)
    {
        DWORD verHandle = 0;
        UINT size = 0;
        LPBYTE lpBuffer = nullptr;
        const DWORD verSize = GetFileVersionInfoSizeA(filePath.string().c_str(), &verHandle);
        std::stringstream versionString;

        if (verSize != NULL)
        {
            const auto verData = new char[ verSize ];

            if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
            {
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
                {
                    if (size)
                    {
                        const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            versionString << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
                                          << static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
                                          << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS));
                        }
                    }
                }
            }
            delete[] verData;
        }

        return semver::version::parse(versionString.str());
    }

    std::expected<void, std::string> ParseCommandLineArguments(argh::parser& cmdl)
    {
        int nArgs;

        LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        // even with no arguments passed, this is expected to succeed
        if (nullptr == szArglist)
        {
            return std::unexpected("CommandLineToArgvW failed to parse the command line");
        }

        auto arglistGuard = sg::make_scope_guard([szArglist]() noexcept { LocalFree(szArglist); });

        std::vector<const char*> argv;
        std::vector<std::string> narrow;

        narrow.reserve(nArgs);
        for (int i = 0; i < nArgs; i++)
        {
            // Windows gives us wide only, convert each to narrow
            narrow.push_back(ConvertWideToANSI(std::wstring(szArglist[ i ])));
        }

        argv.resize(nArgs);
        std::ranges::transform(narrow, argv.begin(), [](const std::string& arg) { return arg.c_str(); });

        argv.push_back(nullptr);

        // we now have the same format as a classic main argv to parse
        cmdl.parse(nArgs, argv.data());

        return {};
    }

    std::string trim(const std::string& str, const std::string& whitespace)
    {
        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos) return "";  // no content

        const auto strEnd = str.find_last_not_of(whitespace);
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

    bool icompare_pred(unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }

    bool icompare(const std::string& a, const std::string& b)
    {
        if (a.length() == b.length())
        {
            return std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
        }
        return false;
    }

    void toCamelCase(std::string& s)
    {
        char previous = ' ';
        auto f = [ & ](char current)
        {
            const char result = (std::isblank(previous) && std::isupper(current)) ? std::tolower(current) : current;
            previous = current;
            return result;
        };
        std::ranges::transform(s, s.begin(), f);
    }

    void toSemVerCompatible(std::string& s)
    {
        const auto notSemver = s;
        // v1.2.3 -> 1.2.3
        if (s.starts_with("v"))
        {
            s.erase(s.begin());
        }
        // for 4 digit version we gonna cheat and convert it to a valid semantic version
        if (std::ranges::count_if(s, [](char c) { return c == '.'; }) > 2)
        {
            s = std::regex_replace(s, std::regex(R"((\d*)\.(\d*)\.(\d*)\.(\d*))"), "$1.$2.$3+$4");
        }
        // Convert traditional non-semver RC/alphas/betas to semver, e.g. `1.0.0-rc1` -> `1.0.0-rc.1`
        s = std::regex_replace(s, std::regex(R"(([a-zA-Z_]+)(\d+))"), "$1.$2");
        // Leading zeroes (e.g. in dates like 2024.08.28) aren't valid in semver
        s = std::regex_replace(s, std::regex(R"(\b0*(\d+))"), "$1");

        spdlog::debug("raw version `{}` -> semver string `{}`", notSemver, s);
    }

    void stripNulls(std::string& s) { s.erase(std::ranges::find(s, '\0'), s.end()); }

    void stripNulls(std::wstring& s) { s.erase(std::ranges::find(s, L'\0'), s.end()); }
}

namespace winapi
{
    semver::version GetWin32ResourceFileVersion(const std::filesystem::path& filePath)
    {
        const auto versionResult = nefarius::winapi::fs::GetFileVersionFromFile(filePath.string());

        if (!versionResult)
        {
            return semver::version{0, 0, 1};
        }

        auto str = to_string(versionResult.value());
        util::toSemVerCompatible(str);

        return semver::version::parse(str);
    }

    semver::version GetWin32ResourceProductVersion(const std::filesystem::path& filePath)
    {
        const auto versionResult = nefarius::winapi::fs::GetProductVersionFromFile(filePath.string());

        if (!versionResult)
        {
            return semver::version{0, 0, 1};
        }

        auto str = to_string(versionResult.value());
        util::toSemVerCompatible(str);
        return semver::version::parse(str);
    }

    std::string GetLastErrorStdStr(DWORD errorCode)
    {
        DWORD error = (errorCode == ERROR_SUCCESS) ? GetLastError() : errorCode;
        if (error)
        {
            LPVOID lpMsgBuf;
            DWORD bufLen =
              FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                             error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, nullptr);
            if (bufLen)
            {
                auto lpMsgStr = static_cast<LPCSTR>(lpMsgBuf);
                std::string result(lpMsgStr, lpMsgStr + bufLen);

                LocalFree(lpMsgBuf);

                return result;
            }
        }

        return {"OK"};
    }

    bool IsMsiExecErrorCode(DWORD errorCode)
    {
        // https://learn.microsoft.com/en-us/windows/win32/msi/error-codes
        std::vector<DWORD> codes{ERROR_INVALID_DATA,
                                 ERROR_INVALID_PARAMETER,
                                 ERROR_CALL_NOT_IMPLEMENTED,
                                 ERROR_APPHELP_BLOCK,
                                 ERROR_INSTALL_SERVICE_FAILURE,
                                 ERROR_INSTALL_USEREXIT,
                                 ERROR_INSTALL_FAILURE,
                                 ERROR_INSTALL_SUSPEND,
                                 ERROR_UNKNOWN_PRODUCT,
                                 ERROR_UNKNOWN_FEATURE,
                                 ERROR_UNKNOWN_COMPONENT,
                                 ERROR_UNKNOWN_PROPERTY,
                                 ERROR_INVALID_HANDLE_STATE,
                                 ERROR_BAD_CONFIGURATION,
                                 ERROR_INDEX_ABSENT,
                                 ERROR_INSTALL_SOURCE_ABSENT,
                                 ERROR_INSTALL_PACKAGE_VERSION,
                                 ERROR_PRODUCT_UNINSTALLED,
                                 ERROR_BAD_QUERY_SYNTAX,
                                 ERROR_INVALID_FIELD,
                                 ERROR_INSTALL_ALREADY_RUNNING,
                                 ERROR_INSTALL_PACKAGE_OPEN_FAILED,
                                 ERROR_INSTALL_PACKAGE_INVALID,
                                 ERROR_INSTALL_UI_FAILURE,
                                 ERROR_INSTALL_LOG_FAILURE,
                                 ERROR_INSTALL_LANGUAGE_UNSUPPORTED,
                                 ERROR_INSTALL_TRANSFORM_FAILURE,
                                 ERROR_INSTALL_PACKAGE_REJECTED,
                                 ERROR_FUNCTION_NOT_CALLED,
                                 ERROR_FUNCTION_FAILED,
                                 ERROR_INVALID_TABLE,
                                 ERROR_DATATYPE_MISMATCH,
                                 ERROR_UNSUPPORTED_TYPE,
                                 ERROR_CREATE_FAILED,
                                 ERROR_INSTALL_TEMP_UNWRITABLE,
                                 ERROR_INSTALL_PLATFORM_UNSUPPORTED,
                                 ERROR_INSTALL_NOTUSED,
                                 ERROR_PATCH_PACKAGE_OPEN_FAILED,
                                 ERROR_PATCH_PACKAGE_INVALID,
                                 ERROR_PATCH_PACKAGE_UNSUPPORTED,
                                 ERROR_PRODUCT_VERSION,
                                 ERROR_INVALID_COMMAND_LINE,
                                 ERROR_INSTALL_REMOTE_DISALLOWED,
                                 ERROR_SUCCESS_REBOOT_INITIATED,
                                 ERROR_PATCH_TARGET_NOT_FOUND,
                                 ERROR_PATCH_PACKAGE_REJECTED,
                                 ERROR_INSTALL_TRANSFORM_REJECTED,
                                 ERROR_INSTALL_REMOTE_PROHIBITED,
                                 ERROR_PATCH_REMOVAL_UNSUPPORTED,
                                 ERROR_UNKNOWN_PATCH,
                                 ERROR_PATCH_NO_SEQUENCE,
                                 ERROR_PATCH_REMOVAL_DISALLOWED,
                                 ERROR_INVALID_PATCH_XML,
                                 ERROR_PATCH_MANAGED_ADVERTISED_PRODUCT,
                                 ERROR_INSTALL_SERVICE_SAFEBOOT,
                                 ERROR_ROLLBACK_DISABLED,
                                 ERROR_INSTALL_REJECTED,
                                 ERROR_SUCCESS_REBOOT_REQUIRED};

        return std::ranges::find(codes, errorCode) != codes.end();
    }

    BOOL VerifyEmbeddedSignature(LPCWSTR pwszSourceFile)
    {
        LONG lStatus;
        DWORD dwLastError;

        // Initialize the WINTRUST_FILE_INFO structure.

        WINTRUST_FILE_INFO FileData;
        memset(&FileData, 0, sizeof(FileData));
        FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
        FileData.pcwszFilePath = pwszSourceFile;
        FileData.hFile = nullptr;
        FileData.pgKnownSubject = nullptr;

        /*
        WVTPolicyGUID specifies the policy to apply on the file
        WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:
        
        1) The certificate used to sign the file chains up to a root 
        certificate located in the trusted root certificate store. This 
        implies that the identity of the publisher has been verified by 
        a certification authority.
        
        2) In cases where user interface is displayed (which this example
        does not do), WinVerifyTrust will check for whether the  
        end entity certificate is stored in the trusted publisher store,  
        implying that the user trusts content from this publisher.
        
        3) The end entity certificate has sufficient permission to sign 
        code, as indicated by the presence of a code signing EKU or no 
        EKU.
        */

        GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA WinTrustData;

        // Initialize the WinVerifyTrust input data structure.

        // Default all fields to 0.
        memset(&WinTrustData, 0, sizeof(WinTrustData));

        WinTrustData.cbStruct = sizeof(WinTrustData);

        // Use default code signing EKU.
        WinTrustData.pPolicyCallbackData = nullptr;

        // No data to pass to SIP.
        WinTrustData.pSIPClientData = nullptr;

        // Disable WVT UI.
        WinTrustData.dwUIChoice = WTD_UI_NONE;

        // No revocation checking.
        WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;

        // Verify an embedded signature on a file.
        WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;

        // Verify action.
        WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;

        // Verification sets this value.
        WinTrustData.hWVTStateData = nullptr;

        // Not used.
        WinTrustData.pwszURLReference = nullptr;

        // This is not applicable if there is no UI because it changes
        // the UI to accommodate running applications instead of
        // installing applications.
        WinTrustData.dwUIContext = 0;

        // Set pFile.
        WinTrustData.pFile = &FileData;

        // WinVerifyTrust verifies signatures as specified by the GUID
        // and Wintrust_Data.
        lStatus = WinVerifyTrust(nullptr, &WVTPolicyGUID, &WinTrustData);

        switch (lStatus)
        {
            case ERROR_SUCCESS:
                /*
            Signed file:
                - Hash that represents the subject is trusted.

                - Trusted publisher without any verification errors.

                - UI was disabled in dwUIChoice. No publisher or 
                    time stamp chain errors.

                - UI was enabled in dwUIChoice and the user clicked 
                    "Yes" when asked to install and run the signed 
                    subject.
            */
                wprintf_s(L"The file \"%s\" is signed and the signature "
                          L"was verified.\n",
                          pwszSourceFile);
                break;

            case TRUST_E_NOSIGNATURE:
                // The file was not signed or had a signature
                // that was not valid.

                // Get the reason for no signature.
                dwLastError = GetLastError();
                if (TRUST_E_NOSIGNATURE == dwLastError || TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
                    TRUST_E_PROVIDER_UNKNOWN == dwLastError)
                {
                    // The file was not signed.
                    wprintf_s(L"The file \"%s\" is not signed.\n", pwszSourceFile);
                }
                else
                {
                    // The signature was not valid or there was an error
                    // opening the file.
                    wprintf_s(L"An unknown error occurred trying to "
                              L"verify the signature of the \"%s\" file.\n",
                              pwszSourceFile);
                }

                break;

            case TRUST_E_EXPLICIT_DISTRUST:
                // The hash that represents the subject or the publisher
                // is not allowed by the admin or user.
                wprintf_s(L"The signature is present, but specifically "
                          L"disallowed.\n");
                break;

            case TRUST_E_SUBJECT_NOT_TRUSTED:
                // The user clicked "No" when asked to install and run.
                wprintf_s(L"The signature is present, but not "
                          L"trusted.\n");
                break;

            case CRYPT_E_SECURITY_SETTINGS:
                /*
            The hash that represents the subject or the publisher 
            was not explicitly trusted by the admin and the 
            admin policy has disabled user trust. No signature, 
            publisher or time stamp errors.
            */
                wprintf_s(L"CRYPT_E_SECURITY_SETTINGS - The hash "
                          L"representing the subject or the publisher wasn't "
                          L"explicitly trusted by the admin and admin policy "
                          L"has disabled user trust. No signature, publisher "
                          L"or timestamp errors.\n");
                break;

            default:
                // The UI was disabled in dwUIChoice or the admin policy
                // has disabled user trust. lStatus contains the
                // publisher or time stamp chain error.
                wprintf_s(L"Error is: 0x%x.\n", lStatus);
                break;
        }

        // Any hWVTStateData must be released by a call with close.
        WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;

        lStatus = WinVerifyTrust(nullptr, &WVTPolicyGUID, &WinTrustData);

        return true;
    }

    std::expected<std::string, std::string> GetUserTemporaryDirectory()
    {
        std::string tempPath(MAX_PATH, '\0');
        // this expands typically to %TEMP% or %LOCALAPPDATA%\Temp
        if (GetTempPathA(MAX_PATH, tempPath.data()) == FALSE)
        {
            // fallback that is also set in 99% of the cases
            if (GetEnvironmentVariableA("TEMP", tempPath.data(), MAX_PATH) == FALSE)
            {
                spdlog::error("Failed to get path to temporary directory, error: {0:#x}", GetLastError());
                return std::unexpected("Failed to resolve temporary directory");
            }
        }

        spdlog::debug("tempPath = {}", tempPath);
        util::stripNulls(tempPath);
        return tempPath;
    }

    std::expected<std::string, std::string> GetNewTemporaryFile(_In_opt_ const std::string& parent)
    {
        std::string tempDir = parent;

        if (parent.empty())
        {
            if (auto r = GetUserTemporaryDirectory(); r)
                tempDir = *r;
            else
                return std::unexpected(r.error());
        }

        std::string tempFile(MAX_PATH, '\0');

        if (GetTempFileNameA(tempDir.c_str(), "VICIUS", 0, tempFile.data()) == FALSE)
        {
            spdlog::error("Failed to get temporary file name, error: {:#x}", GetLastError());
            return std::unexpected(std::format("Failed to create temporary file in '{}': {}", tempDir,
                                               GetLastErrorStdStr()));
        }

        util::stripNulls(tempFile);
        return tempFile;
    }

    std::expected<std::string, std::string> GetProgramDataPath()
    {
        std::string tempPath(MAX_PATH, '\0');

        if (!GetEnvironmentVariableA("ProgramData", tempPath.data(), MAX_PATH))
        {
            spdlog::error("Failed to get path to ProgramData directory, error: {0:#x}", GetLastError());
            return std::unexpected("Failed to resolve %ProgramData% directory");
        }

        spdlog::debug("tempPath = {}", tempPath);
        util::stripNulls(tempPath);
        return tempPath;
    }

    // stolen from: https://building.enlyze.com/posts/writing-win32-apps-like-its-2020-part-3/
    using PGetDpiForMonitor = HRESULT(WINAPI*)(HMONITOR hmonitor, int dpiType, UINT* dpiX, UINT* dpiY);

    WORD GetWindowDPI(HWND hWnd)
    {
        // Try to get the DPI setting for the monitor where the given window is located.
        // This API is Windows 8.1+.
        const HMODULE hShcore = LoadLibraryW(L"shcore");
        if (hShcore)
        {
            const auto pGetDpiForMonitor = reinterpret_cast<PGetDpiForMonitor>(GetProcAddress(hShcore, "GetDpiForMonitor"));
            if (pGetDpiForMonitor)
            {
                const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                UINT uiDpiX;
                UINT uiDpiY;
                const HRESULT hr = pGetDpiForMonitor(hMonitor, 0, &uiDpiX, &uiDpiY);
                if (SUCCEEDED(hr))
                {
                    return static_cast<WORD>(uiDpiX);
                }
            }
        }

        // We couldn't get the window's DPI above, so get the DPI of the primary monitor
        // using an API that is available in all Windows versions.
        const HDC hScreenDC = GetDC(nullptr);
        const int iDpiX = GetDeviceCaps(hScreenDC, LOGPIXELSX);
        ReleaseDC(nullptr, hScreenDC);

        return static_cast<WORD>(iDpiX);
    }

    DWORD GetMonitorRefreshRate(HWND hWnd, DWORD defaultRate)
    {
        const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

        if (!hMonitor)
        {
            return defaultRate;
        }

        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);

        if (!GetMonitorInfo(hMonitor, &monitorInfo))
        {
            return defaultRate;
        }

        DEVMODE devMode;
        ZeroMemory(&devMode, sizeof(devMode));
        devMode.dmSize = sizeof(devMode);

        if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode))
        {
            return devMode.dmDisplayFrequency;
        }

        return defaultRate;
    }

    void SetDarkMode(HWND hWnd, BOOL useDarkMode)
    {
        (void)DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    }

    std::expected<bool, std::string> IsLightThemeActive()
    {
        try
        {
            winreg::RegKey key{
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                KEY_READ
            };
            const DWORD value = key.GetDwordValue(L"AppsUseLightTheme");
            return value != 0;
        }
        catch (const std::exception& e)
        {
            return std::unexpected(std::string{e.what()});
        }
    }

    ImVec4 GetAccentColor()
    {
        // Theme-appropriate fallback accent:
        //   dark  → Fluent Win11 blue #60CDFF
        //   light → Win10/11 default blue #0078D4
        const ImVec4 fallback = IsLightThemeActive().value_or(false)
            ? ImVec4{0.000f, 0.471f, 0.831f, 1.0f}  // #0078D4
            : ImVec4{0.376f, 0.804f, 1.000f, 1.0f};  // #60CDFF

        DWORD color = 0;
        BOOL opaque = FALSE;
        if (FAILED(DwmGetColorizationColor(&color, &opaque)))
            return fallback;

        // DwmGetColorizationColor returns BGRA (0xAARRGGBB in little-endian storage)
        const float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
        const float g = static_cast<float>((color >>  8) & 0xFF) / 255.0f;
        const float b = static_cast<float>((color      ) & 0xFF) / 255.0f;

        // Guard against near-black results (colorization disabled / grey theme)
        if (r < 0.05f && g < 0.05f && b < 0.05f)
            return fallback;

        return ImVec4{r, g, b, 1.0f};
    }

    ImVec4 GetAccentColorHovered()
    {
        const auto a = GetAccentColor();
        // Lighten by blending 20 % toward white
        return ImVec4{
            a.x + (1.0f - a.x) * 0.20f,
            a.y + (1.0f - a.y) * 0.20f,
            a.z + (1.0f - a.z) * 0.20f,
            1.0f
        };
    }

    ImVec4 GetAccentColorActive()
    {
        const auto a = GetAccentColor();
        // Darken by 15 %
        return ImVec4{a.x * 0.85f, a.y * 0.85f, a.z * 0.85f, 1.0f};
    }
}
