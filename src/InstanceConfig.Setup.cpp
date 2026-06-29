#include "pch.h"
#include "InstanceConfig.hpp"
#include "Util.h"


#include <fstream>
#include <functional>
#include <memory>

#include <zip.h>

namespace
{

    template <class T> using c_deleter = std::function<void(T*)>;
    template <class T> using unique_c_deleter_ptr = std::unique_ptr<T, c_deleter<T>>;
    using unique_zip = unique_c_deleter_ptr<zip_t>;
    using unique_zip_file = unique_c_deleter_ptr<zip_file_t>;

    std::string ToUTF8(std::wstring_view view)
    {
        // ICU is preferred, but not using it because:
        // - vicius currently targets Windows 7+, and ICU is only included with some versions of Windows 10
        // - adding it to vcpkg pulls in a *huge* amount of the msys ecosystem as dependencies
        const auto utf8ByteCount =
          WideCharToMultiByte(CP_UTF8, 0, view.data(), static_cast<int>(view.size()), nullptr, 0, nullptr, nullptr);
        if (!utf8ByteCount)
        {
            return {};
        }

        std::string utf8;
        utf8.resize(static_cast<size_t>(utf8ByteCount));
        const auto convertedBytes = WideCharToMultiByte(CP_UTF8, 0, view.data(), static_cast<int>(view.size()), utf8.data(),
                                                        utf8ByteCount, nullptr, nullptr);
        if (!convertedBytes)
        {
            return {};
        }

        if (utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    enum class WaitResult {
        Success,
        Cancelled,
    };

    [[nodiscard]]
    WaitResult CancellableWait(HANDLE object, const std::stop_token& stopToken) {
        HANDLE stopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
        std::stop_callback stopCallback(stopToken, std::bind_front(&SetEvent, stopEvent));

        HANDLE events[] = { object, stopEvent };
        const auto result = WaitForMultipleObjects(std::size(events), events, FALSE, INFINITE);
        CloseHandle(stopEvent);

        if (result == WAIT_OBJECT_0) {
            return WaitResult::Success;
        }

        return WaitResult::Cancelled;
    }
}

bool models::InstanceConfig::InvokeSetupAsync(const std::stop_token& stopToken)
{
    // fail if already in-progress
    if (setupTask.has_value())
    {
        return false;
    }

    // Explicitly copy stopToken so that we don't pass a reference into another thread
    setupTask = std::async(std::launch::async, &InstanceConfig::ExecuteSetup, this, std::stop_token{stopToken});

    return true;
}

void models::InstanceConfig::ResetSetupState() { setupTask.reset(); }

std::optional<models::InstanceConfig::SetupStatus> models::InstanceConfig::GetSetupStatus() const
{
    if (!setupTask.has_value())
    {
        return std::nullopt;
    }

    const std::future_status futureStatus = (*setupTask).wait_for(std::chrono::milliseconds(1));

    SetupStatus status{};
    status.isRunning = futureStatus == std::future_status::timeout;
    status.hasFinished = futureStatus == std::future_status::ready;

    if (status.hasFinished)
    {
        status.result = (*setupTask).get();
    }

    return status;
}

std::expected<std::filesystem::path, std::string> models::InstanceConfig::ExtractReleaseZip(zip_t* zip) const
{
    const auto zipPath = this->GetLocalReleaseTempFilePath();
    std::filesystem::path extractedPath = zipPath;
    if (zipPath.extension() == ".zip" && zipPath.has_stem())
    {
        extractedPath.replace_extension(/* remove extension*/);
    }
    else
    {
        extractedPath.replace_filename(zipPath.filename().string() + "-extracted");
    }


    int zipErrorCode = 0;

    const auto entryCount = zip_get_num_entries(zip, 0);
    zip_stat_t stat;
    for (zip_int64_t i = 0; i < entryCount; ++i)
    {
        zip_stat_init(&stat);
        if (zip_stat_index(zip, i, 0, &stat) != 0)
        {
            const auto zipError = zip_get_error(zip);
            zipErrorCode = zip_error_code_zip(zipError);
            spdlog::error("Failed to stat entry {} in zip: {} ({})", i, zip_error_strerror(zipError), zipErrorCode);
            return std::unexpected(std::format("Failed to stat zip entry {}: {}", i, zip_error_strerror(zipError)));
        }
        constexpr auto requiredFlags = ZIP_STAT_NAME | ZIP_STAT_SIZE;
        if ((stat.valid & requiredFlags) != requiredFlags)
        {
            spdlog::error("Entry {} in zip does not have required metadata.", i);
            return std::unexpected(std::format("Zip entry {} is missing required metadata", i));
        }
        unique_zip_file zipFile(zip_fopen_index(zip, i, 0), &zip_fclose);
        if (!zipFile)
        {
            const auto zipError = zip_get_error(zip);
            spdlog::error("Failed to open file `{}` in zip - skipping: {} ({})", i,
                          ((stat.valid & ZIP_STAT_NAME) ? stat.name : "<UNNAMED>"), zip_error_strerror(zipError),
                          zip_error_code_zip(zipError));
            return std::unexpected(std::format("Failed to open zip entry {}: {}", i, zip_error_strerror(zipError)));
        }

        const auto outPath = (extractedPath / stat.name).lexically_normal();

        // Zip-Slip guard: reject any entry whose normalised path escapes the extraction root
        {
            const auto rel = outPath.lexically_relative(extractedPath.lexically_normal());
            if (rel.empty() || *rel.begin() == L"..")
            {
                spdlog::error("Zip entry '{}' escapes extraction root, skipping", stat.name);
                return std::unexpected(std::format("Zip entry '{}' escapes extraction root", stat.name));
            }
        }

        if (outPath.filename().string().back() == '/')
        {
            std::filesystem::create_directories(outPath);
        }
        else
        {
            try
            {
                std::filesystem::create_directories(outPath.parent_path());

                std::ofstream f(outPath, std::ios::binary);

                constexpr std::size_t chunkSize = 4 * 1024;  // 4KB
                char buffer[ chunkSize ];

                decltype(stat.size) bytesRead = 0;
                while (bytesRead < stat.size)
                {
                    const auto bytesReadThisChunk = zip_fread(zipFile.get(), buffer, std::size(buffer));
                    if (bytesReadThisChunk <= 0)
                    {
                        const auto zipError = zip_file_get_error(zipFile.get());
                        spdlog::error("Failed to read chunk from file in zip: {} ({})", zip_error_strerror(zipError),
                                      zip_error_code_zip(zipError));
                        return std::unexpected(std::format("Failed to read from zip: {}", zip_error_strerror(zipError)));
                    }
                    f << std::string_view(buffer, bytesReadThisChunk);
                    bytesRead += bytesReadThisChunk;
                }
            }
            catch (const std::exception& e)
            {
                spdlog::error("Failed to open output file `{}` when extracting zip: {}", outPath, e.what());
                return std::unexpected(std::format("Failed to write zip entry to '{}': {}", outPath.string(), e.what()));
            }
        }
        if ((stat.valid & ZIP_STAT_MTIME) == ZIP_STAT_MTIME)
        {
            const auto sysTime = std::chrono::system_clock::from_time_t(stat.mtime);
            const auto fileTime = std::chrono::clock_cast<std::chrono::file_clock>(sysTime);
            std::filesystem::last_write_time(outPath, fileTime);
        }
    }
    return extractedPath;
}

/**
 * \brief Spawns the setup of the update release and waits for it to finish.
 * \return SetupResult on success; unexpected error string on failure.
 */
std::expected<models::SetupResult, std::string> models::InstanceConfig::ExecuteSetup(const std::stop_token& stopToken)
{
    if (this->terminateProcessBeforeUpdate.has_value())
    {
        const auto terminatePID = GetProcessId(this->terminateProcessBeforeUpdate.value());
        if (!TerminateProcess(this->terminateProcessBeforeUpdate.value(), 0))
        {
            // TODO: better user feedback; task dialog? Or explicit message in main UI page?
            return std::unexpected(std::format("Failed to terminate process before update: {}",
                                               winapi::GetLastErrorStdStr()));
        }
        spdlog::debug("Terminated PID {} due to {}", terminatePID, NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE);
    }

    const auto& release = this->GetSelectedRelease();
    const auto& tempFile = this->GetLocalReleaseTempFilePath();
    std::string openFile = tempFile.string();
    const bool runAsAdmin = this->RunAsAdmin();

    DWORD win32Error = ERROR_SUCCESS;
    DWORD exitCode = 0;
    bool success = false;
    DWORD binType = 0;

    BOOL isExecutable = GetBinaryTypeA(openFile.c_str(), &binType);

    spdlog::debug("isExecutable = {}, binType = {}", isExecutable, binType);

    //
    // Payload may be MSI, ZIP etc.
    //
    if (!isExecutable)
    {
        int zipErrorCode = 0;
        unique_zip zip(zip_open(ToUTF8(tempFile.wstring()).c_str(), ZIP_RDONLY, &zipErrorCode), &zip_discard);

        //
        // We got an archive, run extraction
        //
        if (zip)
        {
            // Extract the entire zip to a temporary location first, so we don't break the current install if extraction fails
            const auto extractedPath = this->ExtractReleaseZip(zip.get());
            if (!extractedPath)
            {
                return std::unexpected(extractedPath.error());
            }
            {
                using Disposition = ZipExtractFileDisposition;
                const auto sourceRoot = std::filesystem::canonical(*extractedPath);
                const auto destRoot = std::filesystem::canonical(this->GetAppPath().parent_path());

                const auto defaultDisposition =
                  release.zipExtractDefaultFileDisposition.value_or(ZipExtractFileDisposition::CreateOrReplace);
                const auto dispositionOverrides = release.zipExtractFileDispositionOverrides.value_or(
                  std::unordered_map<std::string, ZipExtractFileDisposition>{});

                // Walk 1/2: create or update only, no deletions
                for (const auto& it : std::filesystem::recursive_directory_iterator(sourceRoot))
                {
                    const auto relative = std::filesystem::relative(it.path(), sourceRoot);
                    const auto source = sourceRoot / relative;
                    const auto dest = destRoot / relative;

                    const auto relativeUTF8 = ToUTF8(relative.wstring());
                    const auto disposition = dispositionOverrides.contains(relativeUTF8) ? dispositionOverrides.at(relativeUTF8)
                                                                                         : defaultDisposition;

                    // Containment check: relative path must not escape destRoot
                    {
                        const auto relCheck = std::filesystem::relative(dest, destRoot);
                        if (relCheck.empty() || *relCheck.begin() == "..")
                        {
                            spdlog::warn("Skipping entry '{}': resolved path escapes installation directory", relativeUTF8);
                            continue;
                        }
                    }

                    if (it.is_directory())
                    {
                        if (!std::filesystem::exists(dest))
                        {
                            std::filesystem::create_directories(dest);
                            std::filesystem::last_write_time(dest, std::filesystem::last_write_time(source));
                        }
                        else if (disposition == Disposition::CreateOrReplace)
                        {
                            std::filesystem::last_write_time(dest, std::filesystem::last_write_time(source));
                        }
                        continue;
                    }

                    switch (disposition)
                    {
                        case Disposition::DeleteIfPresent:
                            // Handled below when walking deletions
                            break;
                        case Disposition::CreateIfAbsent:
                            std::filesystem::copy_file(sourceRoot / relative, destRoot / relative,
                                                       std::filesystem::copy_options::skip_existing);
                            break;
                        case Disposition::CreateOrReplace:
                            std::filesystem::copy_file(sourceRoot / relative, destRoot / relative,
                                                       std::filesystem::copy_options::overwrite_existing);
                            break;
                    }
                    std::filesystem::last_write_time(dest, std::filesystem::last_write_time(source));
                }
                // Walk 2/2: deletions
                for (const auto& [ relative, disposition ] : dispositionOverrides)
                {
                    if (disposition != Disposition::DeleteIfPresent)
                    {
                        continue;
                    }
                    const auto dest = destRoot / relative;
                    if (!std::filesystem::exists(dest))
                    {
                        continue;
                    }
                    {
                        std::error_code ec;
                        const auto canonDest = std::filesystem::canonical(dest, ec);
                        const auto relCheck = ec ? std::filesystem::path{} : std::filesystem::relative(canonDest, destRoot);
                        if (ec || relCheck.empty() || *relCheck.begin() == "..")
                        {
                            spdlog::warn("Cowardly refusing to delete '{}': path escapes installation directory", relative);
                            continue;
                        }
                    }
                    std::filesystem::remove_all(destRoot / relative);
                }

                success = true;
            }
        }
        //
        // Most probably an MSI or similar, offload execution to the default shell launch action
        //
        else
        {
            std::string args = release.launchArguments.has_value() ? release.launchArguments.value() : std::string{};

            SHELLEXECUTEINFOA execInfo = {};
            execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
            execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
            execInfo.hwnd = this->windowHandle;
            execInfo.lpVerb = runAsAdmin ? "runas" : NULL;
            execInfo.lpFile = openFile.c_str();
            execInfo.lpParameters = args.c_str();
            execInfo.lpDirectory = NULL;
            execInfo.nShow = SW_SHOW;
            execInfo.hInstApp = NULL;

            if (!ShellExecuteExA(&execInfo))
            {
                win32Error = GetLastError();

                spdlog::error("Failed to launch {}, error {:#x}, message {}", tempFile, win32Error,
                              winapi::GetLastErrorStdStr());

                goto exit;
            }


            if (CancellableWait(execInfo.hProcess, stopToken) == WaitResult::Success) {
                GetExitCodeProcess(execInfo.hProcess, &exitCode);
            } else {
                exitCode = this->GetSuccessExitCode(NV_S_CLOSED_WHILE_UPDATER_RUNNING);
            }
            CloseHandle(execInfo.hProcess);
        }
    }
    //
    // Executables: if elevation is requested use ShellExecuteEx ("runas"),
    // otherwise spawn directly via CreateProcess.
    //
    else if (runAsAdmin)
    {
        const std::string args = release.launchArguments.value_or(std::string{});

        SHELLEXECUTEINFOA execInfo = {};
        execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        execInfo.hwnd = this->windowHandle;
        execInfo.lpVerb = "runas";
        execInfo.lpFile = openFile.c_str();
        execInfo.lpParameters = args.c_str();
        execInfo.lpDirectory = NULL;
        execInfo.nShow = SW_SHOW;
        execInfo.hInstApp = NULL;

        if (!ShellExecuteExA(&execInfo))
        {
            win32Error = GetLastError();

            spdlog::error("Failed to launch elevated {}, error {:#x}, message {}", tempFile, win32Error,
                          winapi::GetLastErrorStdStr());

            goto exit;
        }

        spdlog::debug("Elevated setup process launched successfully");

        if (CancellableWait(execInfo.hProcess, stopToken) == WaitResult::Success)
        {
            GetExitCodeProcess(execInfo.hProcess, &exitCode);
        }
        else
        {
            exitCode = this->GetSuccessExitCode(NV_S_CLOSED_WHILE_UPDATER_RUNNING);
        }
        CloseHandle(execInfo.hProcess);
    }
    else
    {
        STARTUPINFOA info = {};
        info.cb = sizeof(STARTUPINFOA);
        PROCESS_INFORMATION updateProcessInfo = {};

        std::stringstream launchArgs;
        launchArgs << tempFile;

        if (release.launchArguments.has_value())
        {
            launchArgs << " " << release.launchArguments.value();
        }

        std::string args = launchArgs.str();
        // CreateProcess may write into the command line buffer; never pass string::c_str().
        std::vector<char> cmdLine(args.begin(), args.end());
        cmdLine.push_back('\0');

        if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &info,
                            &updateProcessInfo))
        {
            win32Error = GetLastError();

            spdlog::error("Failed to launch {}, error {:#x}, message {}", tempFile, win32Error,
                          winapi::GetLastErrorStdStr());

            goto exit;
        }

        spdlog::debug("Setup process launched successfully");

        if (CancellableWait(updateProcessInfo.hProcess, stopToken) == WaitResult::Success) {
            GetExitCodeProcess(updateProcessInfo.hProcess, &exitCode);
        } else {
            exitCode = this->GetSuccessExitCode(NV_S_CLOSED_WHILE_UPDATER_RUNNING);
        }
        CloseHandle(updateProcessInfo.hProcess);
        CloseHandle(updateProcessInfo.hThread);
    }

    //
    // Run exit code validation, if configured
    //
    if (const auto exitCodeCheck = this->ExitCodeCheck(); exitCodeCheck.has_value())
    {
        const auto [ skipCheck, successCodes ] = exitCodeCheck.value();

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
    // Could not even launch the setup (win32Error is set by the goto exit paths).
    if (win32Error != ERROR_SUCCESS)
    {
        return std::unexpected(std::format("Setup failed: {} (win32Error={:#x})",
                                           winapi::GetLastErrorStdStr(win32Error), win32Error));
    }

    // Process ran to completion — carry the real exit code whether it succeeded or not.
    return SetupResult{.exitCode = exitCode, .win32Error = win32Error, .succeeded = success};
}
