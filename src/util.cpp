#include "pch.h"
#include "Common.h"
#include <Softpub.h>
#include <wintrust.h>


EXTERN_C IMAGE_DOS_HEADER __ImageBase;


namespace util
{
    std::filesystem::path GetImageBasePathW()
    {
        wchar_t myPath[MAX_PATH + 1] = {0};

        GetModuleFileNameW(
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            myPath,
            MAX_PATH + 1
        );

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
            const auto verData = new char[verSize];

            if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
            {
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
                {
                    if (size)
                    {
                        const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            versionString
                                << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
                                << static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
                                << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS));
                        }
                    }
                }
            }
            delete[] verData;
        }

        return semver::version{versionString.str()};
    }

    bool ParseCommandLineArguments(argh::parser& cmdl)
    {
        int nArgs;

        const LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        // even with no arguments passed, this is expected to succeed
        if (nullptr == szArglist)
        {
            return false;
        }

        std::vector<const char*> argv;
        std::vector<std::string> narrow;

        for (int i = 0; i < nArgs; i++)
        {
            // Windows gives us wide only, convert each to narrow
            narrow.push_back(ConvertWideToANSI(std::wstring(szArglist[i])));
        }

        argv.resize(nArgs);
        std::ranges::transform(narrow, argv.begin(), [](const std::string& arg) { return arg.c_str(); });

        argv.push_back(nullptr);

        // we now have the same format as a classic main argv to parse
        cmdl.parse(nArgs, argv.data());

        return true;
    }

    std::string trim(const std::string& str, const std::string& whitespace)
    {
        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos)
            return ""; // no content

        const auto strEnd = str.find_last_not_of(whitespace);
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

    bool icompare_pred(unsigned char a, unsigned char b)
    {
        return std::tolower(a) == std::tolower(b);
    }

    bool icompare(const std::string& a, const std::string& b)
    {
        if (a.length() == b.length())
        {
            return std::equal(b.begin(), b.end(),
                              a.begin(), icompare_pred);
        }
        return false;
    }

    bool IsAdmin(int& errorCode)
    {
        BOOL isAdmin = FALSE;

        if (winapi::IsAppRunningAsAdminMode(&isAdmin) != ERROR_SUCCESS)
        {
            errorCode = EXIT_FAILURE;
            return false;
        }

        if (!isAdmin)
        {
            errorCode = EXIT_FAILURE;
            return false;
        }

        return true;
    }

    void toCamelCase(std::string& s)
    {
        char previous = ' ';
        auto f = [&](char current)
        {
            const char result = (std::isblank(previous) && std::isupper(current))
                                    ? std::tolower(current)
                                    : current;
            previous = current;
            return result;
        };
        std::ranges::transform(s, s.begin(), f);
    }

    void toSemVerCompatible(std::string& s)
    {
        // for 4 digit version we gonna cheat and convert it to a valid semantic version
        if (std::ranges::count_if(s, [](char c) { return c == '.'; }) > 2)
        {
            s = std::regex_replace(
                s,
                std::regex(R"((\d*)\.(\d*)\.(\d*)\.(\d*))"),
                "$1.$2.$3-rc.$4"
            );
        }
    }

    void stripNulls(std::string& s)
    {
        s.erase(std::ranges::find(s, '\0'), s.end());
    }

    void stripNulls(std::wstring& s)
    {
        s.erase(std::ranges::find(s, L'\0'), s.end());
    }
}

namespace winapi
{
    semver::version GetWin32ResourceFileVersion(const std::filesystem::path& filePath)
    {
        DWORD verHandle = 0;
        UINT size = 0;
        LPBYTE lpBuffer = nullptr;
        const DWORD verSize = GetFileVersionInfoSizeA(filePath.string().c_str(), &verHandle);
        std::stringstream versionString;

        if (verSize != NULL)
        {
            const auto verData = new char[verSize];

            if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
            {
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
                {
                    if (size)
                    {
                        const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            versionString
                                << static_cast<ULONG>(HIWORD(verInfo->dwFileVersionMS)) << "."
                                << static_cast<ULONG>(LOWORD(verInfo->dwFileVersionMS)) << "."
                                << static_cast<ULONG>(HIWORD(verInfo->dwFileVersionLS)) << "."
                                << static_cast<ULONG>(LOWORD(verInfo->dwFileVersionLS));
                        }
                    }
                }
            }
            delete[] verData;
        }

        return semver::version{versionString.str()};
    }

    semver::version GetWin32ResourceProductVersion(const std::filesystem::path& filePath)
    {
        DWORD verHandle = 0;
        UINT size = 0;
        LPBYTE lpBuffer = nullptr;
        const DWORD verSize = GetFileVersionInfoSizeA(filePath.string().c_str(), &verHandle);
        std::stringstream versionString;

        if (verSize != NULL)
        {
            const auto verData = new char[verSize];

            if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
            {
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
                {
                    if (size)
                    {
                        const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            versionString
                                << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
                                << static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
                                << static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS)) << "."
                                << static_cast<ULONG>(LOWORD(verInfo->dwProductVersionLS));
                        }
                    }
                }
            }
            delete[] verData;
        }

        return semver::version{versionString.str()};
    }

    DWORD IsAppRunningAsAdminMode(PBOOL IsAdmin)
    {
        DWORD dwError = ERROR_SUCCESS;
        PSID pAdministratorsGroup = nullptr;

        // Allocate and initialize a SID of the administrators group.
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
        if (!AllocateAndInitializeSid(
            &NtAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &pAdministratorsGroup))
        {
            dwError = GetLastError();
            goto Cleanup;
        }

        // Determine whether the SID of administrators group is enabled in 
        // the primary access token of the process.
        if (!CheckTokenMembership(nullptr, pAdministratorsGroup, IsAdmin))
        {
            dwError = GetLastError();
        }

    Cleanup:
        // Centralized cleanup for all allocated resources.
        if (pAdministratorsGroup)
        {
            FreeSid(pAdministratorsGroup);
            pAdministratorsGroup = nullptr;
        }

        return dwError;
    }

    std::string GetLastErrorStdStr(DWORD errorCode)
    {
        DWORD error = (errorCode == ERROR_SUCCESS) ? GetLastError() : errorCode;
        if (error)
        {
            LPVOID lpMsgBuf;
            DWORD bufLen = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&lpMsgBuf,
                0, nullptr);
            if (bufLen)
            {
                auto lpMsgStr = static_cast<LPCSTR>(lpMsgBuf);
                std::string result(lpMsgStr, lpMsgStr + bufLen);

                LocalFree(lpMsgBuf);

                return result;
            }
        }
        return std::string("OK");
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
        lStatus = WinVerifyTrust(
            nullptr,
            &WVTPolicyGUID,
            &WinTrustData);

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
            if (TRUST_E_NOSIGNATURE == dwLastError ||
                TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError ||
                TRUST_E_PROVIDER_UNKNOWN == dwLastError)
            {
                // The file was not signed.
                wprintf_s(L"The file \"%s\" is not signed.\n",
                          pwszSourceFile);
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
            wprintf_s(L"Error is: 0x%x.\n",
                      lStatus);
            break;
        }

        // Any hWVTStateData must be released by a call with close.
        WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;

        lStatus = WinVerifyTrust(
            nullptr,
            &WVTPolicyGUID,
            &WinTrustData);

        return true;
    }
}
