#include "pch.h"
#include "Common.h"
#include "Util.h"
#include "InstanceConfig.hpp"


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
    const HANDLE self =
      CreateFileA(appPath.string().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    const DWORD error = GetLastError();

    if (self != INVALID_HANDLE_VALUE) CloseHandle(self);

    // error is ERROR_SHARING_VIOLATION when we can write
    // and ERROR_ACCESS_DENIED if missing permissions

    return error == ERROR_SHARING_VIOLATION;
}

std::expected<void, std::string> models::InstanceConfig::RunSelfUpdater() const
{
    const auto workDir = appPath.parent_path();
    std::stringstream dllPath;
    dllPath << appPath.string() << NV_ADS_UPDATER_NAME;
    const auto ads = dllPath.str();
    spdlog::debug("ads = {}", ads);

    const auto runDll = "rundll32.exe";

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

    // if we can write to our directory, spawn under current user
    if (HasWritePermissions())
    {
        spdlog::debug("Running with regular privileges");

        STARTUPINFOA si = {sizeof(STARTUPINFOA)};
        PROCESS_INFORMATION pi{};

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::stringstream argsStream;
        argsStream << "rundll32 \"" << ads << "\",PerformUpdate"
                   << " --silent"
                   << " --log-level " << magic_enum::enum_name(spdlog::get_level())
                   << " --pid " << GetCurrentProcessId()
                   << " --path \"" << appPath.string() << "\""
                   << " --url \"" << latestUrl << "\""
                   << checksumArgs;
        const auto args = argsStream.str();
        spdlog::debug("args = {}", args);

        if (!CreateProcessA(nullptr,
                            const_cast<LPSTR>(args.c_str()),
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
            spdlog::error("Failed to run updater process, error: {0:#x} ({})", launchError, launchErrorMsg);
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
                   << " --path \"" << appPath.string() << "\""
                   << " --url \"" << latestUrl << "\""
                   << checksumArgs;
        const auto args = argsStream.str();
        spdlog::debug("args = {}", args);

        SHELLEXECUTEINFOA shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = nullptr;
        shExInfo.lpVerb = "runas";
        shExInfo.lpFile = runDll;
        shExInfo.lpParameters = args.c_str();
        shExInfo.lpDirectory = workDir.string().c_str();
        shExInfo.nShow = SW_HIDE;
        shExInfo.hInstApp = nullptr;

        if (!ShellExecuteExA(&shExInfo))
        {
            const DWORD launchError = GetLastError();
            const auto launchErrorMsg = winapi::GetLastErrorStdStr(launchError);
            spdlog::error("Failed to run elevated updater process, error: {0:#x} ({})", launchError, launchErrorMsg);
            return std::unexpected(std::format("Failed to launch elevated self-updater process: {}", launchErrorMsg));
        }

        spdlog::debug("Process launched");

        CloseHandle(shExInfo.hProcess);
    }

    return {};
}
