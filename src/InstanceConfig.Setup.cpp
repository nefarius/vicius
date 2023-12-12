#include "pch.h"
#include "InstanceConfig.hpp"
#include "Util.h"


bool models::InstanceConfig::InvokeSetupAsync()
{
    // fail if already in-progress
    if (setupTask.has_value())
    {
        return false;
    }

    setupTask = std::async(
        std::launch::async,
        &InstanceConfig::ExecuteSetup,
        this
    );

    return true;
}

void models::InstanceConfig::ResetSetupState()
{
    setupTask.reset();
}

bool models::InstanceConfig::GetSetupStatus(
    bool& isRunning,
    bool& hasFinished,
    bool& hasSucceeded,
    DWORD& statusCode,
    DWORD& win32Error
) const
{
    if (!setupTask.has_value())
    {
        return false;
    }

    const std::future_status status = (*setupTask).wait_for(std::chrono::milliseconds(1));

    isRunning = status == std::future_status::timeout;
    hasFinished = status == std::future_status::ready;

    if (hasFinished)
    {
        const auto result = (*setupTask).get();

        hasSucceeded = std::get<0>(result);
        statusCode = std::get<1>(result);
        win32Error = std::get<2>(result);
    }

    return true;
}

/**
 * \brief Spawns the setup of the update release and waits for it to finish.
 * \return The Win32 error code.
 */
std::tuple<bool, DWORD, DWORD> models::InstanceConfig::ExecuteSetup()
{
    const auto& release = this->GetSelectedRelease();
    const auto& tempFile = this->GetLocalReleaseTempFilePath();

    std::stringstream launchArgs;
    launchArgs << tempFile;

    if (release.launchArguments.has_value())
    {
        launchArgs << " " << release.launchArguments.value();
    }

    const auto& args = launchArgs.str();

    DWORD status = ERROR_SUCCESS;
    DWORD exitCode = 0;
    bool success = false;

    if (release.useShellExecute)
    {
        SHELLEXECUTEINFOA execInfo = {};
        execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        execInfo.hwnd = NULL;
        execInfo.lpVerb = NULL;
        execInfo.lpFile = nullptr;
        execInfo.lpParameters = args.c_str();
        execInfo.lpDirectory = NULL;
        execInfo.nShow = SW_SHOW;
        execInfo.hInstApp = NULL;

        if (!ShellExecuteExA(&execInfo))
        {
            status = GetLastError();

            spdlog::error("Failed to launch {}, error {:#x}, message {}",
                          tempFile.string(), status, winapi::GetLastErrorStdStr());

            goto exit;
        }

        WaitForSingleObject(execInfo.hProcess, INFINITE);
        GetExitCodeProcess(execInfo.hProcess, &exitCode);
        CloseHandle(execInfo.hProcess);
    }
    else
    {
        STARTUPINFOA info = {};
        info.cb = sizeof(STARTUPINFOA);
        PROCESS_INFORMATION updateProcessInfo = {};

        if (!CreateProcessA(
            nullptr,
            const_cast<LPSTR>(args.c_str()),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &info,
            &updateProcessInfo
        ))
        {
            status = GetLastError();

            spdlog::error("Failed to launch {}, error {:#x}, message {}",
                          tempFile.string(), status, winapi::GetLastErrorStdStr());

            goto exit;
        }

        spdlog::debug("Setup process launched successfully");

        WaitForSingleObject(updateProcessInfo.hProcess, INFINITE);
        GetExitCodeProcess(updateProcessInfo.hProcess, &exitCode);
        CloseHandle(updateProcessInfo.hProcess);
        CloseHandle(updateProcessInfo.hThread);
    }

    //
    // Run exit code validation, if configured
    // 
    if (this->ExitCodeCheck().has_value())
    {
        const auto [skipCheck, successCodes] = this->ExitCodeCheck().value();

        if (skipCheck)
        {
            spdlog::debug("Skipping error code check as per configuration");

            success = true;
        }

        if (std::ranges::find(successCodes, exitCode) != successCodes.end())
        {
            spdlog::debug("Exit code {} marked as success-condition", exitCode);

            success = true;
        }
    }

    //
    // Fallback if nothing is configured
    // 
    if (!success)
    {
        success = exitCode == NV_SUCCESS_EXIT_CODE;
    }

exit:
    return std::make_tuple(success, exitCode, status);
}
