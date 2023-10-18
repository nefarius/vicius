// dll.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "dll.h"

static std::string ConvertWideToANSI(const std::wstring& wstr)
{
    int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), static_cast<int>(wstr.length()), nullptr, 0, nullptr,
                                    nullptr);
    std::string str(count, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
    return str;
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
        "--pid", // PID of the parent process
        "--url", // latest updater download URL
        "--path", // the target file path
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

    std::filesystem::path original = cmdl({"--path"}).str();
    spdlog::debug("original = {}", original.string());
    const auto workDir = original.parent_path();
    // hint: we must remain on the same drive, or renaming will fail!
    std::filesystem::path randomName(GetRandomString());
    spdlog::debug("randomName = {}", randomName.string());
    std::filesystem::path temp = original.parent_path() / randomName;
    std::string tempFile = temp.string();
    spdlog::debug("tempFile = {}", tempFile);
    curlpp::Cleanup myCleanup;
    HANDLE hProcess = nullptr;
    int retries = 20; // 5 seconds timeout

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

        if (hProcess)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(hProcess, &exitCode);

            if (exitCode == 0)
            {
                spdlog::debug("Process exited with code {}", exitCode);
                CloseHandle(hProcess);
                break;
            }

            Sleep(250);
        }
    }
    while (hProcess);

    spdlog::debug("Preparing download");

    std::ofstream outStream;
    const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
    outStream.exceptions(exceptionMask);

    try
    {
        // we can not yet directly write to it but move it to free the original name!
        MoveFileA(original.string().c_str(), tempFile.c_str());
        SetFileAttributesA(tempFile.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        spdlog::debug("Moved file {} to hidden file {}", original.string(), tempFile);

        spdlog::debug("Starting download");
        // download directly to main file stream
        outStream.open(original, std::ios::binary | std::ofstream::ate);
        outStream << curlpp::options::Url(url);

        spdlog::info("Downloading {} finished", url);

        if (DeleteFileA(tempFile.c_str()) == 0)
        {
            spdlog::warn("Failed to delete file {}, scheduling removal on reboot", tempFile);

            // if it still fails, schedule nuking the old file at next reboot
            MoveFileExA(
                tempFile.c_str(),
                nullptr,
                MOVEFILE_DELAY_UNTIL_REBOOT
            );
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
            TRUE,
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
        // welp
    }

    // restore original file on failure
    CopyFileA(tempFile.c_str(), original.string().c_str(), FALSE);
    SetFileAttributesA(original.string().c_str(), FILE_ATTRIBUTE_NORMAL);
    // attempt to delete temporary copy
    if (DeleteFileA(tempFile.c_str()) == 0)
    {
        // if it still fails, schedule nuking the old file at next reboot
        MoveFileExA(
            tempFile.c_str(),
            nullptr,
            MOVEFILE_DELAY_UNTIL_REBOOT
        );
    }

    spdlog::warn("Finished with warning or errors");
}
