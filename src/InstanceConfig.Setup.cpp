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
}

bool models::InstanceConfig::InvokeSetupAsync()
{
    // fail if already in-progress
    if (setupTask.has_value())
    {
        return false;
    }

    setupTask = std::async(std::launch::async, &InstanceConfig::ExecuteSetup, this);

    return true;
}

void models::InstanceConfig::ResetSetupState() { setupTask.reset(); }

bool models::InstanceConfig::GetSetupStatus(bool& isRunning, bool& hasFinished, bool& hasSucceeded, DWORD& exitCode,
                                            DWORD& win32Error) const
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
        exitCode = std::get<1>(result);
        win32Error = std::get<2>(result);
    }

    return true;
}

std::optional<std::filesystem::path> models::InstanceConfig::ExtractReleaseZip(zip_t* zip) const
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
            return std::nullopt;
        }
        constexpr auto requiredFlags = ZIP_STAT_NAME | ZIP_STAT_SIZE;
        if ((stat.valid & requiredFlags) != requiredFlags)
        {
            spdlog::error("Entry {} in zip does not have required metadata.", i);
            return std::nullopt;
        }
        unique_zip_file zipFile(zip_fopen_index(zip, i, 0), &zip_fclose);
        if (!zipFile)
        {
            const auto zipError = zip_get_error(zip);
            spdlog::error("Failed to open file `{}` in zip - skipping: {} ({})", i,
                          ((stat.valid & ZIP_STAT_NAME) ? stat.name : "<UNNAMED>"), zip_error_strerror(zipError),
                          zip_error_code_zip(zipError));
            return std::nullopt;
        }

        const auto outPath = extractedPath / stat.name;
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

                constexpr std::size_t chunkSize = 4 * 1024; // 4KB
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
                        return std::nullopt;
                    }
                    f << std::string_view(buffer, bytesReadThisChunk);
                    bytesRead += bytesReadThisChunk;
                }
            }
            catch (const std::exception& e)
            {
                spdlog::error("Failed to open output file `{}` when extracting zip: {}", outPath.string(), e.what());
                return std::nullopt;
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
 * \return Tuple of success boolean, process exit code and Win32 error code.
 */
std::tuple<bool, DWORD, DWORD> models::InstanceConfig::ExecuteSetup()
{
    const auto& release = this->GetSelectedRelease();
    const auto& tempFile = this->GetLocalReleaseTempFilePath();
    std::string openFile = tempFile.string();

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
            if (extractedPath)
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
                    const auto disposition = dispositionOverrides.contains(relativeUTF8)
                                                 ? dispositionOverrides.at(relativeUTF8)
                                                 : defaultDisposition;

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
                    if (!std::filesystem::canonical(dest).wstring().starts_with(destRoot.wstring()))
                    {
                        spdlog::warn("Cowardly refusing to delete files outside of the installation directory");
                    }
                    std::filesystem::remove_all(destRoot / relative);
                }
            }
            success = true;
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
            execInfo.lpVerb = NULL;
            execInfo.lpFile = openFile.c_str();
            execInfo.lpParameters = args.c_str();
            execInfo.lpDirectory = NULL;
            execInfo.nShow = SW_SHOW;
            execInfo.hInstApp = NULL;

            if (!ShellExecuteExA(&execInfo))
            {
                win32Error = GetLastError();

                spdlog::error("Failed to launch {}, error {:#x}, message {}", tempFile.string(), win32Error,
                              winapi::GetLastErrorStdStr());

                goto exit;
            }

            WaitForSingleObject(execInfo.hProcess, INFINITE);
            GetExitCodeProcess(execInfo.hProcess, &exitCode);
            CloseHandle(execInfo.hProcess);
        }
    }
    //
    // Executables can directly be spawned via CreateProcess
    // 
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

        const auto& args = launchArgs.str();

        if (!CreateProcessA(nullptr,
                            const_cast<LPSTR>(args.c_str()),
                            nullptr,
                            nullptr,
                            TRUE,
                            0,
                            nullptr,
                            nullptr,
                            &info,
                            &updateProcessInfo))
        {
            win32Error = GetLastError();

            spdlog::error("Failed to launch {}, error {:#x}, message {}", tempFile.string(), win32Error,
                          winapi::GetLastErrorStdStr());

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
    return std::make_tuple(success, exitCode, win32Error);
}
