// dll.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "dll.h"
#include <wintrust.h>
#include <softpub.h>
#include <bcrypt.h>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "bcrypt.lib")

static std::string ConvertWideToANSI(const std::wstring& wstr)
{
    int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), static_cast<int>(wstr.length()), nullptr, 0, nullptr,
                                    nullptr);
    std::string str(count, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
    return str;
}

// ============================================================================
// Checksum helpers (BCrypt-based, no external hash library dependency)
// ============================================================================

static std::expected<std::string, std::string> ComputeFileSHA256(const std::filesystem::path& filePath)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<BYTE> hashObj, hashBuf;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return std::unexpected("BCryptOpenAlgorithmProvider failed");

    auto cleanAlg = [&]{ if (hAlg) { BCryptCloseAlgorithmProvider(hAlg, 0); hAlg = nullptr; } };

    DWORD objLen = 0, hashLen = 0, result = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(DWORD), &result, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptGetProperty(OBJECT_LENGTH) failed"); }
    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(DWORD), &result, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptGetProperty(HASH_LENGTH) failed"); }

    hashObj.resize(objLen);
    hashBuf.resize(hashLen);

    if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), objLen, nullptr, 0, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptCreateHash failed"); }

    auto cleanHash = [&]{ if (hHash) { BCryptDestroyHash(hHash); hHash = nullptr; } };

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) { cleanHash(); cleanAlg(); return std::unexpected(std::format("Failed to open '{}' for hashing", filePath.string())); }

    constexpr std::size_t chunkSize = 65536;
    std::vector<BYTE> buf(chunkSize);
    for (;;)
    {
        file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(chunkSize));
        const std::streamsize n = file.gcount();
        if (n > 0 && BCryptHashData(hHash, buf.data(), static_cast<ULONG>(n), 0) != 0)
        { cleanHash(); cleanAlg(); return std::unexpected("BCryptHashData failed"); }
        if (!file)
        {
            if (!file.eof()) { cleanHash(); cleanAlg(); return std::unexpected("I/O error reading file for hashing"); }
            break;
        }
    }

    if (BCryptFinishHash(hHash, hashBuf.data(), hashLen, 0) != 0)
    { cleanHash(); cleanAlg(); return std::unexpected("BCryptFinishHash failed"); }

    cleanHash();
    cleanAlg();

    std::string hex;
    hex.reserve(hashLen * 2);
    for (DWORD i = 0; i < hashLen; ++i)
    {
        char nibbles[3];
        std::snprintf(nibbles, sizeof(nibbles), "%02x", hashBuf[i]);
        hex += nibbles;
    }
    return hex;
}

static std::expected<std::string, std::string> ComputeFileSHA1(const std::filesystem::path& filePath)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<BYTE> hashObj, hashBuf;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0)
        return std::unexpected("BCryptOpenAlgorithmProvider failed");

    auto cleanAlg = [&]{ if (hAlg) { BCryptCloseAlgorithmProvider(hAlg, 0); hAlg = nullptr; } };

    DWORD objLen = 0, hashLen = 0, result = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(DWORD), &result, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptGetProperty(OBJECT_LENGTH) failed"); }
    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(DWORD), &result, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptGetProperty(HASH_LENGTH) failed"); }

    hashObj.resize(objLen);
    hashBuf.resize(hashLen);

    if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), objLen, nullptr, 0, 0) != 0)
    { cleanAlg(); return std::unexpected("BCryptCreateHash failed"); }

    auto cleanHash = [&]{ if (hHash) { BCryptDestroyHash(hHash); hHash = nullptr; } };

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) { cleanHash(); cleanAlg(); return std::unexpected(std::format("Failed to open '{}' for hashing", filePath.string())); }

    constexpr std::size_t chunkSize = 65536;
    std::vector<BYTE> buf(chunkSize);
    for (;;)
    {
        file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(chunkSize));
        const std::streamsize n = file.gcount();
        if (n > 0 && BCryptHashData(hHash, buf.data(), static_cast<ULONG>(n), 0) != 0)
        { cleanHash(); cleanAlg(); return std::unexpected("BCryptHashData failed"); }
        if (!file)
        {
            if (!file.eof()) { cleanHash(); cleanAlg(); return std::unexpected("I/O error reading file for hashing"); }
            break;
        }
    }

    if (BCryptFinishHash(hHash, hashBuf.data(), hashLen, 0) != 0)
    { cleanHash(); cleanAlg(); return std::unexpected("BCryptFinishHash failed"); }

    cleanHash();
    cleanAlg();

    std::string hex;
    hex.reserve(hashLen * 2);
    for (DWORD i = 0; i < hashLen; ++i)
    {
        char nibbles[3];
        std::snprintf(nibbles, sizeof(nibbles), "%02x", hashBuf[i]);
        hex += nibbles;
    }
    return hex;
}

// ============================================================================
// Authenticode helpers (WinVerifyTrust - revocation checked, no lifetime flag)
// ============================================================================

static std::expected<void, std::string> VerifyAuthenticode(const std::wstring& filePath)
{
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = filePath.c_str();
    fileInfo.hFile = nullptr;
    fileInfo.pgKnownSubject = nullptr;

    GUID wvtPolicyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA wtData = {};
    wtData.cbStruct = sizeof(WINTRUST_DATA);
    wtData.pPolicyCallbackData = nullptr;
    wtData.pSIPClientData = nullptr;
    wtData.dwUIChoice = WTD_UI_NONE;
    wtData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    wtData.dwUnionChoice = WTD_CHOICE_FILE;
    wtData.pFile = &fileInfo;
    wtData.dwStateAction = WTD_STATEACTION_VERIFY;
    wtData.hWVTStateData = nullptr;
    wtData.pwszURLReference = nullptr;
    // Do NOT set WTD_LIFETIME_SIGNING so time-stamped expired certs remain valid
    wtData.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN;
    wtData.dwUIContext = 0;

    const LONG lStatus = WinVerifyTrust(nullptr, &wvtPolicyGuid, &wtData);

    // Release state data
    wtData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &wvtPolicyGuid, &wtData);

    if (lStatus != ERROR_SUCCESS)
        return std::unexpected(std::format("WinVerifyTrust failed (status=0x{:08X})", static_cast<unsigned long>(lStatus)));
    return {};
}

// ============================================================================
// Case-insensitive hex string comparison (for checksum matching)
// ============================================================================
static bool IHexEqual(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(),
                      [](unsigned char ca, unsigned char cb)
                      { return std::tolower(ca) == std::tolower(cb); });
}

static std::string GetRandomString()
{
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

    std::random_device rd;
    std::mt19937 generator(rd());

    std::ranges::shuffle(str, generator);

    return str.substr(0, 8);
}


EXTERN_C DLL_API void CALLBACK PerformUpdate(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    int nArgs;

    const LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    // even with no arguments passed, this is expected to succeed
    if (nullptr == szArglist)
    {
        return;
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

    argh::parser cmdl;

    cmdl.add_params({
        "--pid",           // PID of the parent process
        "--url",           // latest updater download URL
        "--path",          // the target file path
        "--checksum",      // expected SHA256 hex digest of downloaded file
        "--checksum-alg",  // checksum algorithm: sha256 (default), sha1
        "--log-level"
    });

    // we now have the same format as a classic main argv to parse
    cmdl.parse(nArgs, argv.data());

    const auto logLevel = magic_enum::enum_cast<spdlog::level::level_enum>(cmdl("--log-level").str());

    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);

    // override log level, if provided by CLI
    if (logLevel.has_value())
    {
        sink->set_level(logLevel.value());
    }
    else
    {
        sink->set_level(spdlog::level::debug);
    }

    auto logger = std::make_shared<spdlog::logger>("vicius-self-updater", sink);

    // override log level, if provided by CLI
    if (logLevel.has_value())
    {
        logger->set_level(logLevel.value());
        logger->flush_on(logLevel.value());
    }
    else
    {
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
    }

    set_default_logger(logger);

    bool silent = cmdl[{"--silent"}];
    spdlog::debug("silent = {}", silent);

    std::string url;
    if (!(cmdl({"--url"}) >> url))
    {
        spdlog::critical("--url parameter missing");
        return;
    }

    int pid;
    if (!(cmdl({"--pid"}) >> pid))
    {
        spdlog::critical("--pid parameter missing");
        return;
    }

    if (!cmdl({"--path"}))
    {
        spdlog::critical("--path parameter missing");
        return;
    }

    // Optional integrity parameters
    const std::string expectedChecksum = cmdl({"--checksum"}).str();
    const std::string checksumAlg = [&]
    {
        const std::string alg = cmdl({"--checksum-alg"}).str();
        return alg.empty() ? std::string("sha256") : alg;
    }();

    if (!expectedChecksum.empty())
    {
        spdlog::info("Checksum verification requested: alg={} expected={}", checksumAlg, expectedChecksum);
    }
    else
    {
        spdlog::warn("No --checksum provided; self-update will proceed without checksum verification");
    }

    std::filesystem::path original = cmdl({"--path"}).str();
    spdlog::debug("original = {}", original.string());
    const auto workDir = original.parent_path();

    // Backup name - remains on same drive so rename is atomic
    std::filesystem::path backupName(GetRandomString() + ".bak");
    std::filesystem::path backup = workDir / backupName;
    std::string backupFile = backup.string();
    spdlog::debug("backup = {}", backupFile);

    // Download to a separate temp file first, then verify before swapping
    std::filesystem::path downloadName(GetRandomString() + ".tmp");
    std::filesystem::path downloadPath = workDir / downloadName;
    std::string downloadFile = downloadPath.string();
    spdlog::debug("downloadFile = {}", downloadFile);

    curlpp::Cleanup myCleanup;
    HANDLE hProcess = nullptr;
    int retries = 20; // 2 seconds timeout

    // wait until parent is no more
    do
    {
        if (hProcess) CloseHandle(hProcess);

        // we failed and bail since the rest of the logic will not work
        if (--retries < 1)
        {
            spdlog::critical("Waiting for process with PID {} to end timed out", pid);
            return;
        }

        hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION,
            FALSE,
            pid
        );

        spdlog::debug("hProcess = {}", fmt::ptr(hProcess));
        spdlog::debug("GetLastError = {}", GetLastError());

        if (hProcess)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(hProcess, &exitCode);

            spdlog::debug("exitCode = {}", exitCode);

            if (exitCode == ERROR_SUCCESS || exitCode == 201 /* NV_S_SELF_UPDATER */)
            {
                spdlog::debug("Process exited with code {}", exitCode);
                CloseHandle(hProcess);
                break;
            }

            Sleep(100);
        }
    }
    while (hProcess);

    spdlog::debug("Preparing download to temp file");

    std::ofstream outStream;
    const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
    outStream.exceptions(exceptionMask);

    auto RestoreBackup = [&]()
    {
        // Clean up the failed download temp file first (it is always disposable).
        if (std::filesystem::exists(downloadPath))
        {
            if (DeleteFileA(downloadFile.c_str()) == FALSE)
            {
                MoveFileExA(downloadFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            }
        }

        // Restore original from backup.  Only remove the backup after a confirmed copy;
        // if CopyFileA fails, keep the backup intact so the user can recover manually.
        if (std::filesystem::exists(backup))
        {
            if (CopyFileA(backupFile.c_str(), original.string().c_str(), FALSE) != FALSE)
            {
                SetFileAttributesA(original.string().c_str(), FILE_ATTRIBUTE_NORMAL);
                spdlog::info("Restored backup to {}", original.string());

                // Backup is no longer needed - remove it.
                if (DeleteFileA(backupFile.c_str()) == FALSE)
                {
                    MoveFileExA(backupFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }
            else
            {
                spdlog::error("CRITICAL: failed to restore {} from backup {} (error: {}). "
                              "The backup is preserved at that path for manual recovery.",
                              original.string(), backupFile, GetLastError());
                // Do NOT delete the backup - it is the only good copy left.
            }
        }
    };

    try
    {
        // Move the original to a hidden backup (frees the target filename for the final rename)
        MoveFileA(original.string().c_str(), backupFile.c_str());
        SetFileAttributesA(backupFile.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        spdlog::debug("Moved original {} to backup {}", original.string(), backupFile);

        // Download to the separate temp file (NOT the original location yet)
        spdlog::info("Downloading {} to {}", url, downloadFile);
        outStream.open(downloadPath, std::ios::binary | std::ofstream::ate);
        outStream << curlpp::options::Url(url);
        outStream.close();
        spdlog::info("Download finished");

        // ----------------------------------------------------------------
        // Integrity verification before swap
        // ----------------------------------------------------------------

        // 1. Checksum verification
        if (!expectedChecksum.empty())
        {
            std::expected<std::string, std::string> hashResult;

            if (checksumAlg == "sha256" || checksumAlg.empty())
            {
                hashResult = ComputeFileSHA256(downloadPath);
            }
            else if (checksumAlg == "sha1")
            {
                hashResult = ComputeFileSHA1(downloadPath);
            }
            else
            {
                spdlog::error("Unknown checksum algorithm: {}", checksumAlg);
                RestoreBackup();
                if (!silent)
                    MessageBoxA(hwnd, "Unknown checksum algorithm in self-update arguments.",
                                "Self-update verification failed", MB_ICONERROR | MB_OK);
                return;
            }

            if (!hashResult)
            {
                spdlog::error("Failed to compute checksum of downloaded file {}: {}", downloadFile, hashResult.error());
                RestoreBackup();
                if (!silent)
                    MessageBoxA(hwnd, "Failed to compute checksum of the downloaded updater binary.",
                                "Self-update verification failed", MB_ICONERROR | MB_OK);
                return;
            }

            spdlog::debug("Checksum: computed={} expected={}", *hashResult, expectedChecksum);

            if (!IHexEqual(*hashResult, expectedChecksum))
            {
                spdlog::error("Checksum MISMATCH: computed {} != expected {}", *hashResult, expectedChecksum);
                RestoreBackup();
                if (!silent)
                    MessageBoxA(hwnd,
                                "The downloaded updater binary checksum does not match the expected value.\n"
                                "The download may have been corrupted or tampered with.",
                                "Self-update verification failed", MB_ICONERROR | MB_OK);
                return;
            }

            spdlog::info("Checksum verification PASSED");
        }

        // 2. Authenticode / WinVerifyTrust verification
        const auto sigResult = VerifyAuthenticode(downloadPath.wstring());

        if (!sigResult)
        {
            // An unsigned binary is treated as suspicious in self-update context
            spdlog::error("Authenticode verification FAILED for downloaded updater binary {}: {}", downloadFile, sigResult.error());
            RestoreBackup();
            if (!silent)
                MessageBoxA(hwnd,
                            "The downloaded updater binary does not have a valid Authenticode signature.\n"
                            "Self-update has been aborted for security reasons.",
                            "Self-update verification failed", MB_ICONERROR | MB_OK);
            return;
        }

        spdlog::info("Authenticode verification PASSED");

        // ----------------------------------------------------------------
        // Verification passed - swap into place
        // ----------------------------------------------------------------
        if (!MoveFileA(downloadFile.c_str(), original.string().c_str()))
        {
            spdlog::error("Failed to rename {} to {} (error: {})",
                          downloadFile, original.string(), GetLastError());
            RestoreBackup();
            return;
        }

        spdlog::info("Swapped verified download into {}", original.string());

        // Clean up the backup
        if (DeleteFileA(backupFile.c_str()) == FALSE)
        {
            spdlog::warn("Failed to delete backup {}, scheduling removal on reboot", backupFile);
            MoveFileExA(backupFile.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }

        spdlog::info("Spawning main process install procedure");

        STARTUPINFOA si = {sizeof(STARTUPINFOA)};
        PROCESS_INFORMATION pi{};

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::stringstream argsStream;
        // build CLI args
        argsStream
            << original // main executable
            << " --install" // install steps might have changed in new version
            << " --skip-self-update"; // extra protection to not end up in a loop
        const auto launchArgs = argsStream.str();
        spdlog::debug("launchArgs = {}", launchArgs);

        if (!CreateProcessA(
            nullptr,
            const_cast<LPSTR>(launchArgs.c_str()),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workDir.string().c_str(),
            &si,
            &pi
        ))
        {
            spdlog::error("Failed to run main process, error: {}", GetLastError());
            return;
        }

        spdlog::debug("Process launched");

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        spdlog::info("Finished successfully, exiting self-updater");
        return;
    }
    catch (curlpp::RuntimeError& e)
    {
        spdlog::error("Runtime error: {}", e.what());
        if (!silent) MessageBoxA(hwnd, e.what(), "Runtime error", MB_ICONERROR | MB_OK);
    }
    catch (curlpp::LogicError& e)
    {
        spdlog::error("Logic error: {}", e.what());
        if (!silent) MessageBoxA(hwnd, e.what(), "Logic error", MB_ICONERROR | MB_OK);
    }
    catch (std::ios_base::failure& e)
    {
        spdlog::error("I/O error: {}", e.what());
        if (!silent) MessageBoxA(hwnd, e.what(), "I/O error", MB_ICONERROR | MB_OK);
    }
    catch (...)
    {
        spdlog::error("Unknown error during self-update");
    }

    // Restore original on any unhandled failure
    RestoreBackup();
    spdlog::warn("Finished with errors - original binary restored");
}
