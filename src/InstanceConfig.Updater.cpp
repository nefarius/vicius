#include "pch.h"
#include "Common.h"
#include "Http.h"
#include "Util.h"
#include "InstanceConfig.hpp"

using winapi::QuoteArg;


std::expected<void, std::string> models::InstanceConfig::ExtractSelfUpdater() const
{
    const HRSRC updater_res = FindResource(appInstance, MAKEINTRESOURCE(IDR_DLL_SELF_UPDATER), RT_RCDATA);
    if (!updater_res)
    {
        return std::unexpected(winapi::GetLastErrorStdStr());
    }

    const HGLOBAL updater_global = LoadResource(appInstance, updater_res);
    if (!updater_global)
    {
        return std::unexpected(winapi::GetLastErrorStdStr());
    }

    const int updater_size = static_cast<int>(SizeofResource(appInstance, updater_res));
    if (updater_size <= 0)
    {
        return std::unexpected("Self-updater resource is empty");
    }

    const LPVOID updater_data = LockResource(updater_global);
    if (!updater_data)
    {
        return std::unexpected("Failed to lock self-updater resource");
    }

    std::stringstream ss;
    ss << appPath.string() << NV_ADS_UPDATER_NAME;
    const auto ads = ss.str();

    const HANDLE self = CreateFileA(ads.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (self == INVALID_HANDLE_VALUE)
    {
        return std::unexpected(winapi::GetLastErrorStdStr());
    }

    DWORD bytesWritten = 0;

    // write DLL resource to our self as ADS
    if (const auto ret = WriteFile(self, updater_data, updater_size, &bytesWritten, nullptr);
        !ret || bytesWritten < static_cast<DWORD>(updater_size))
    {
        const DWORD error = GetLastError();
        CloseHandle(self);
        SetLastError(error);
        return std::unexpected(winapi::GetLastErrorStdStr());
    }

    CloseHandle(self);

    return {};
}

bool models::InstanceConfig::HasWritePermissions() const
{
    // Explicit, non-destructive probe: try to create-and-immediately-delete a uniquely
    // named empty file in our own directory. This directly tests what actually matters
    // (can we write new files there, e.g. the self-updater ADS and, post-swap, the main
    // binary) instead of inferring it from a sharing violation on the running exe itself,
    // which only proves the exe is locked, not that the directory is writable, and would
    // report false for a perfectly writable, currently-unlocked location.
    const std::string dir = appPath.parent_path().string();

    char probePath[MAX_PATH]{};
    if (GetTempFileNameA(dir.c_str(), "NVW", 0, probePath) == 0)
    {
        spdlog::debug("HasWritePermissions: GetTempFileNameA in '{}' failed, error {:#x}", dir, GetLastError());
        return false;
    }

    // GetTempFileNameA(..., uUnique=0, ...) already created the (empty) file as a side
    // effect of generating a unique name; remove it immediately so the probe leaves no trace.
    // Treat a failed delete as non-writable: create-without-delete would leave debris and
    // is not a reliable signal that we can replace files here (e.g. delete-denied ACLs).
    if (DeleteFileA(probePath) == FALSE)
    {
        spdlog::debug("HasWritePermissions: DeleteFileA('{}') failed, error {:#x}", probePath, GetLastError());
        return false;
    }

    return true;
}

std::expected<void, std::string> models::InstanceConfig::RunSelfUpdater() const
{
    const auto workDir = appPath.parent_path();
    std::stringstream dllPath;
    dllPath << appPath.string() << NV_ADS_UPDATER_NAME;
    const auto ads = dllPath.str();
    spdlog::debug("ads = {}", ads);

    // Resolve rundll32.exe to its fully-qualified %SystemRoot%\System32 path and pass it
    // as an explicit application name (lpApplicationName / SHELLEXECUTEINFO::lpFile) rather
    // than letting CreateProcess/ShellExecuteEx resolve a bare "rundll32" through the
    // default DLL/EXE search order (which can include the current directory).
    const auto rundll32Path = winapi::GetSystemExecutablePath("rundll32.exe");
    if (!rundll32Path)
    {
        return std::unexpected(std::format("Failed to resolve rundll32.exe: {}", rundll32Path.error()));
    }
    const auto& runDll = *rundll32Path;

    const auto& inst = remote.instance.value();
    const std::string latestUrl = inst.latestUrl.value();

    // Build optional integrity args to pass to the self-updater DLL.
    // The self-updater verifies them before swapping the binary into place.
    std::string checksumArgs;
    if (inst.latestChecksum.has_value())
    {
        const auto& cs = inst.latestChecksum.value();
        checksumArgs = std::format(" --checksum \"{}\" --checksum-alg \"{}\"",
                                   cs.checksum,
                                   magic_enum::enum_name(cs.checksumAlg));
        spdlog::info("Passing checksum to self-updater: alg={} value={}",
                     magic_enum::enum_name(cs.checksumAlg), cs.checksum);
    }
    else
    {
        spdlog::warn("No checksum available for self-updater binary; verification will be Authenticode-only");
    }

    // Build mirror URL args (passed as repeated --mirror-url).
    std::string mirrorArgs;
    if (inst.latestMirrorUrls.has_value())
    {
        for (const auto& m : inst.latestMirrorUrls.value())
            mirrorArgs += " --mirror-url " + QuoteArg(m);
    }

    // Build network args (proxy, DoH) from the sidecar NetworkConfig.
    std::string networkArgs;
    {
        const web::HttpGetOptions netOpts = BuildBaseHttpOptions(latestUrl);
        if (netOpts.proxy.has_value())
        {
            if (!netOpts.proxy->empty())
                networkArgs += " --proxy " + QuoteArg(*netOpts.proxy);
            else
                networkArgs += " --no-proxy"; // ProxyMode::None — force direct in the DLL
        }
        if (!netOpts.dohUrl.empty())
            networkArgs += " --doh-url " + QuoteArg(netOpts.dohUrl);
    }

    // if we can write to our directory, spawn under current user
    if (HasWritePermissions())
    {
        spdlog::debug("Running with regular privileges");

        STARTUPINFOA si = {sizeof(STARTUPINFOA)};
        PROCESS_INFORMATION pi{};

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::stringstream argsStream;
        argsStream << QuoteArg(runDll) << " \"" << ads << "\",PerformUpdate"
                   << " --silent"
                   << " --log-level " << magic_enum::enum_name(spdlog::get_level())
                   << " --pid " << GetCurrentProcessId()
                   << " --path " << QuoteArg(appPath.string())
                   << " --url "  << QuoteArg(latestUrl)
                   << checksumArgs
                   << mirrorArgs
                   << networkArgs;
        const auto args = argsStream.str();
        spdlog::debug("args = {}", args);

        // lpApplicationName pins the executable to the resolved system path so it can
        // never be resolved ambiguously via PATH/CWD search order; the command line's
        // argv[0] is kept consistent with it for well-behaved child argument parsing.
        // CreateProcessA may write into lpCommandLine while parsing/expanding arguments,
        // so it must be a writable buffer, never a std::string's internal storage.
        auto cmdLine = winapi::MakeWritableCommandLine(args);

        if (!CreateProcessA(runDll.c_str(),
                            cmdLine.data(),
                            nullptr,
                            nullptr,
                            TRUE,
                            CREATE_NO_WINDOW,
                            nullptr,
                            workDir.string().c_str(),
                            &si,
                            &pi))
        {
            const DWORD launchError = GetLastError();
            const auto launchErrorMsg = winapi::GetLastErrorStdStr(launchError);
            spdlog::error("Failed to run updater process, error: {:#x} ({})", launchError, launchErrorMsg);
            return std::unexpected(std::format("Failed to launch self-updater process: {}", launchErrorMsg));
        }

        spdlog::debug("Process launched");

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    // request elevation
    else
    {
        spdlog::debug("Requesting running with elevated privileges");

        TryDisplayUACDialog();

        std::stringstream argsStream;
        argsStream << "\"" << ads << "\",PerformUpdate"
                   << " --silent"
                   << " --log-level " << magic_enum::enum_name(spdlog::get_level())
                   << " --pid " << GetCurrentProcessId()
                   << " --path " << QuoteArg(appPath.string())
                   << " --url "  << QuoteArg(latestUrl)
                   << checksumArgs
                   << mirrorArgs
                   << networkArgs;
        const auto args = argsStream.str();
        spdlog::debug("args = {}", args);

        SHELLEXECUTEINFOA shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = nullptr;
        shExInfo.lpVerb = "runas";
        shExInfo.lpFile = runDll.c_str();
        shExInfo.lpParameters = args.c_str();
        shExInfo.lpDirectory = workDir.string().c_str();
        shExInfo.nShow = SW_HIDE;
        shExInfo.hInstApp = nullptr;

        if (!ShellExecuteExA(&shExInfo))
        {
            const DWORD launchError = GetLastError();
            const auto launchErrorMsg = winapi::GetLastErrorStdStr(launchError);
            spdlog::error("Failed to run elevated updater process, error: {:#x} ({})", launchError, launchErrorMsg);
            return std::unexpected(std::format("Failed to launch elevated self-updater process: {}", launchErrorMsg));
        }

        spdlog::debug("Process launched");

        CloseHandle(shExInfo.hProcess);
    }

    return {};
}
