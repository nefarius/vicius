#include "pch.h"
#include "Common.h"
#include "Util.h"
#include "WizardPage.h"
#include "InstanceConfig.hpp"
#include "DownloadAndInstall.hpp"


//
// Enable visual styles
// 
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#define AS_MB	(1024 * 1024)

extern ImFont* G_Font_H1;
extern ImFont* G_Font_H2;
extern ImFont* G_Font_H3;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(szCmdLine);
    UNREFERENCED_PARAMETER(iCmdShow);

    argh::parser cmdl;

    cmdl.add_params({
        NV_CLI_PARAM_LOG_LEVEL,
        NV_CLI_PARAM_LOG_TO_FILE,
        NV_CLI_PARAM_SERVER_URL,
        NV_CLI_PARAM_CHANNEL,
        NV_CLI_PARAM_ADD_HEADER,
        NV_CLI_PARAM_OVERRIDE_OK
    });

    if (!util::ParseCommandLineArguments(cmdl))
    {
        // TODO: better fallback action?
        spdlog::critical("Failed to parse command line arguments");
        return NV_E_CLI_PARSING;
    }

    // updater configuration, defaults and app state
    models::InstanceConfig cfg(hInstance, cmdl);

#pragma region Install command

    // actions to perform when install is instructed
#if !defined(NV_FLAGS_ALWAYS_RUN_INSTALL)
    if (!cmdl[{NV_CLI_TEMPORARY}] && cmdl[{NV_CLI_INSTALL}])
    {
#endif
        if (!cmdl[{NV_CLI_NO_AUTOSTART}])
        {
            if (const auto autoRet = cfg.RegisterAutostart(); !std::get<0>(autoRet))
            {
                // TODO: better fallback action?

                spdlog::critical("Failed to register in autostart");
                cfg.TryDisplayErrorDialog("Failed to register in autostart", std::get<1>(autoRet));
                return NV_E_AUTOSTART;
            }
        }

        if (!cmdl[{NV_CLI_NO_SCHEDULED_TASK}])
        {
            if (const auto taskRet = cfg.CreateScheduledTask(); FAILED(std::get<0>(taskRet)))
            {
                // TODO: better fallback action?

                _com_error err(std::get<0>(taskRet));
                spdlog::critical("Failed to (re-)create Scheduled Task, error: {}, HRESULT: {}", std::get<1>(taskRet),
                                 ConvertWideToANSI(err.ErrorMessage()));
                cfg.TryDisplayErrorDialog("Failed to create scheduled task", std::get<1>(taskRet));
                return NV_E_SCHEDULED_TASK;
            }
        }

        if (const auto extRet = cfg.ExtractSelfUpdater(); !std::get<0>(extRet))
        {
            // TODO: better fallback action?

            spdlog::critical("Failed to extract self-updater, error: {}", std::get<1>(extRet));
            cfg.TryDisplayErrorDialog("Failed to extract self-updater", std::get<1>(extRet));
            return NV_E_EXTRACT_SELF_UPDATE;
        }

#if !defined(NV_FLAGS_ALWAYS_RUN_INSTALL)
        spdlog::info("Installation tasks finished successfully");

        int successCode = NV_S_INSTALL;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);
        return successCode;
    }
#endif

#pragma endregion

#pragma region Autostart tasks

    // actions to perform when running in autostart
    if (!cmdl[{NV_CLI_TEMPORARY}] && cmdl[{NV_CLI_AUTOSTART}])
    {
        if (const auto ret = cfg.CreateScheduledTask(); FAILED(std::get<0>(ret)))
        {
            _com_error err(std::get<0>(ret));
            spdlog::error("Failed to (re-)create Scheduled Task, error: {}, HRESULT: {}", std::get<1>(ret),
                          ConvertWideToANSI(err.ErrorMessage()));
            cfg.TryDisplayErrorDialog("Failed to create scheduled task", std::get<1>(ret));

            // TODO: anything else we can do in this case?
        }
    }

#pragma endregion

#pragma region Uninstall command

    // uninstall tasks
    if (!cmdl[{NV_CLI_TEMPORARY}] && cmdl[{NV_CLI_UNINSTALL}])
    {
        if (const auto autoRet = cfg.RemoveAutostart(); !std::get<0>(autoRet))
        {
            // TODO: better fallback action?

            spdlog::critical("Failed to de-register from autostart");
            cfg.TryDisplayErrorDialog("Failed to de-register from autostart", std::get<1>(autoRet));
            return NV_E_AUTOSTART;
        }

        if (const auto taskRet = cfg.RemoveScheduledTask(); FAILED(std::get<0>(taskRet)))
        {
            // TODO: better fallback action?

            spdlog::critical("Failed to delete scheduled task, error: {}", std::get<1>(taskRet));
            cfg.TryDisplayErrorDialog("Failed to delete scheduled task", std::get<1>(taskRet));
            return NV_E_SCHEDULED_TASK;
        }

        int successCode = NV_S_INSTALL;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);
        return successCode;
    }

#pragma endregion

    // purge postpone data, if any
    if (cmdl[{NV_CLI_PURGE_POSTPONE}])
    {
        int successCode = NV_S_POSTPONE_PURGE;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);

        return cfg.PurgePostponeData()
                   ? successCode
                   : NV_E_POSTPONE_PURGE_FAILED;
    }

    // contact update server and get latest state and config
    if (const auto ret = cfg.RequestUpdateInfo(); !std::get<0>(ret))
    {
        // TODO: add fallback actions

        spdlog::critical("Failed to get server response");
        cfg.TryDisplayErrorDialog("Failed to get server response", std::get<1>(ret));
        return NV_E_SERVER_RESPONSE;
    }

    // launches emergency URL in default browser, if any
    if (cfg.HasEmergencyUrlSet())
    {
        cfg.LaunchEmergencySite();
    }

    // check for updater updates - updateception :D
    if (!cmdl[{NV_CLI_TEMPORARY}] && (!cmdl[{NV_CLI_SKIP_SELF_UPDATE}] && cfg.IsNewerUpdaterAvailable()))
    {
        spdlog::debug("Newer updater version available, invoking self-update");

        if (cfg.RunSelfUpdater())
        {
            return NV_S_SELF_UPDATER;
        }

        spdlog::error("Failed to invoke self-update");
    }

    // restart ourselves from a temporary location, if requested
    if (cfg.TryRunTemporaryProcess())
    {
        int successCode = NV_S_LAUNCHED_TEMPORARY;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);
        return successCode;
    }

    bool isOutdated = false;
    // run local product detection
    if (const auto ret = cfg.IsInstalledVersionOutdated(isOutdated); !std::get<0>(ret))
    {
        // TODO: add error handling

        spdlog::critical("Failed to detect installed product version");
        cfg.TryDisplayErrorDialog("Failed to detect installed product version", std::get<1>(ret));
        return NV_E_PRODUCT_DETECTION;
    }

    // we're up2date and silent, exit
    if (!isOutdated)
    {
        spdlog::info("Installed software is up-to-date");
        cfg.TryDisplayUpToDateDialog();
        int successCode = NV_S_UP_TO_DATE;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);
        return successCode;
    }

    // there's a pending update but user chose to postpone
    if (cfg.IsInPostponePeriod())
    {
        spdlog::info("Postpone period active, exiting");
        int successCode = NV_S_POSTPONE_PERIOD;
        (cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> successCode);
        return successCode;
    }

    // check if we are currently bothering the user
    if (!cmdl[{NV_CLI_IGNORE_BUSY_STATE}] && cfg.IsSilent())
    {
        // query state for the next 30 minutes before giving up
        int retries = 30;

    retryBusy:
        QUERY_USER_NOTIFICATION_STATE state = {};

        if (const HRESULT hr = SHQueryUserNotificationState(&state); FAILED(hr))
        {
            spdlog::warn("Querying notification state failed with HRESULT {}", hr);
        }
        else
        {
            if (state != QUNS_ACCEPTS_NOTIFICATIONS)
            {
                if (--retries < 1)
                {
                    spdlog::info("User busy or running full-screen game, exiting");
                    return NV_E_BUSY;
                }

                // wait for roughly a minute
                Sleep(60 * 1000);
                goto retryBusy;
            }
        }
    }

    //
    // Main GUI creation
    // 

    constexpr int windowWidth = 640, windowHeight = 512;
    sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), cfg.GetWindowTitle(), sf::Style::Titlebar);
    HWND hWnd = window.getSystemHandle();

    window.setFramerateLimit(winapi::GetMonitorRefreshRate(hWnd));
    ImGui::SFML::Init(window, false);

    // disable unused features
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // get DPI scale    
    auto dpi = winapi::GetWindowDPI(hWnd);
    auto scaleFactor = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    auto scaledWidth = (windowWidth * scaleFactor);
    auto scaledHeight = (windowHeight * scaleFactor);
    window.setSize(sf::Vector2u(static_cast<uint32_t>(scaledWidth), static_cast<uint32_t>(scaledHeight)));
    io.DisplaySize = ImVec2(scaledWidth, scaledHeight);

    ui::LoadFonts(hInstance, 16, scaleFactor);
    ui::ApplyImGuiStyleDark(scaleFactor);

    // Set window icon
    if (auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN)))
    {
        SendMessage(hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
    }

    window.setVisible(false);
    winapi::SetDarkMode(hWnd);
    window.setVisible(true);

    cfg.SetWindowHandle(hWnd);

    // workaround for
    // - https://github.com/nefarius/vicius/issues/46
    // - https://github.com/SFML/imgui-sfml/issues/206
    // - https://github.com/SFML/imgui-sfml/issues/212
    SetFocus(hWnd);
    ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::LostFocus));
    ImGui::SFML::ProcessEvent(window, sf::Event(sf::Event::GainedFocus));

    // TODO: try best compromise to display window when user is busy
    //SendMessage(window.getSystemHandle(), WM_SYSCOMMAND, SC_MINIMIZE, 0);

    auto currentPage = WizardPage::Start;
    auto instStep = DownloadAndInstallStep::Begin;
    bool isBackDisabled = false;
    bool isCancelDisabled = false;
    DWORD status = ERROR_SUCCESS;
    std::once_flag errorUrlTriggered;

    sf::Clock deltaClock;
    while (window.isOpen())
    {
        sf::Event event{};
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(window, event);

            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar;

        // fakes a little window border/margin
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x + 5, mainViewport->WorkPos.y + 5));
        ImGui::SetNextWindowSize(ImVec2(scaledWidth - 10, scaledHeight - 10));

        ImGui::Begin("MainWindow", nullptr, flags);

        if (currentPage == WizardPage::Start)
        {
            isBackDisabled = true;
        }

        ImGui::BeginDisabled(isBackDisabled);
        if (ImGui::SmallButton(ICON_FK_ARROW_LEFT))
        {
            --currentPage;

            if (currentPage == WizardPage::MultipleVersionsOverview && cfg.HasSingleRelease())
            {
                --currentPage;
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::Text("Found Updates for %s", cfg.GetProductName().c_str());

        float navigateButtonOffsetY = mainViewport->WorkSize.y - (42 * scaleFactor);
        float leftBorderIndent = 40.0;

        switch (currentPage)
        {
        case WizardPage::Start:
            {
                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30 * scaleFactor));
                ImGui::Text("Updates for %s are available", cfg.GetProductName().c_str());
                ImGui::PopFont();

                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H2);

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30 * scaleFactor));
                if (ImGui::Button(ICON_FK_DOWNLOAD " Display update details now"))
                {
                    currentPage = cfg.HasSingleRelease()
                                      ? WizardPage::SingleVersionSummary
                                      : WizardPage::MultipleVersionsOverview;
                }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (20 * scaleFactor));
                if (ImGui::Button(ICON_FK_CLOCK_O " Remind me tomorrow"))
                {
                    cfg.SetPostponeData();
                    status = NV_S_USER_POSTPONED;
                    window.close();
                }

                if (cfg.GetHelpUrl().has_value())
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (20 * scaleFactor));
                    if (ImGui::Button(ICON_FK_QUESTION " Open help web page"))
                    {
                        ShellExecuteA(nullptr, "open", cfg.GetHelpUrl().value().c_str(), nullptr, nullptr,
                                      SW_SHOWNORMAL);
                    }
                }

                ImGui::PopFont();
                ImGui::Unindent(leftBorderIndent);
                ImGui::Unindent(leftBorderIndent);
                break;
            }
        case WizardPage::SingleVersionSummary:
            {
                isBackDisabled = false;

                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30 * scaleFactor));
                ImGui::Text("Update Summary");
                ImGui::PopFont();

                const auto& release = cfg.GetSelectedRelease();
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
                ImGui::BeginChild(
                    "Summary",
                    ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - (60 * scaleFactor)),
                    false,
                    windowFlags
                );
                markdown::RenderChangelog(
                    release.summary.empty()
                        ? "This release contains no summary."
                        : release.summary
                );
                ImGui::EndChild();

                ImGui::SetCursorPos(ImVec2(mainViewport->WorkSize.x - (215 * scaleFactor), navigateButtonOffsetY));
                if (ImGui::Button("Download and install"))
                {
                    instStep = DownloadAndInstallStep::Begin;
                    currentPage = WizardPage::DownloadAndInstall;
                }

                ImGui::Unindent(leftBorderIndent);

                break;
            }
        case WizardPage::MultipleVersionsOverview:
            {
                isBackDisabled = false;

                // TODO: implement me! The idea behind this page is to offer a list of releases for the user to choose from
                break;
            }
        case WizardPage::DownloadAndInstall:
            {
                static DWORD lastExitCode = 0;
                static double totalToDownload = 0;
                static double totalDownloaded = 0;

                // use this state to reset everything since the user might retry on error
                if (instStep == DownloadAndInstallStep::Begin)
                {
                    isBackDisabled = true;
                    isCancelDisabled = true;

                    totalToDownload = 0;
                    totalDownloaded = 0;

                    cfg.ResetReleaseDownloadState();
                    cfg.ResetSetupState();
                }

                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30 * scaleFactor));
                ImGui::Text("Installing Updates");
                ImGui::PopFont();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (30 * scaleFactor));

                bool isDownloading = false;
                bool hasFinished = false;
                int statusCode = -1;

                // checks if a download is currently running or has been invoked
                if (!cfg.GetReleaseDownloadStatus(isDownloading, hasFinished, statusCode))
                {
                    lastExitCode = 0;
                    totalToDownload = 0;
                    totalDownloaded = 0;

                    // start download
                    cfg.DownloadReleaseAsync(
                        cfg.GetSelectedReleaseId(),
                        [](void* pData, double downloadTotal, double downloaded, double uploadTotal,
                           double uploaded) -> int
                        {
                            UNREFERENCED_PARAMETER(pData);
                            UNREFERENCED_PARAMETER(uploadTotal);
                            UNREFERENCED_PARAMETER(uploaded);

                            totalToDownload = downloadTotal;
                            totalDownloaded = downloaded;

                            return CURLE_OK;
                        });

                    instStep = DownloadAndInstallStep::Downloading;
                }

                // download has finished, advance step
                if (instStep == DownloadAndInstallStep::Downloading && hasFinished)
                {
                    spdlog::debug("Download finished with status code {}", statusCode);
                    instStep = statusCode == httplib::OK_200
                                   ? DownloadAndInstallStep::DownloadSucceeded
                                   : DownloadAndInstallStep::DownloadFailed;
                }

                switch (instStep)
                {
                case DownloadAndInstallStep::Downloading:

                    if (totalDownloaded <= 0 || totalToDownload <= 0)
                    {
                        ImGui::Text("Starting download...");
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (5 * scaleFactor));
                        ui::IndeterminateProgressBar(
                            ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
                    }
                    else
                    {
                        ImGui::Text("Downloading (%.2f MB of %.2f MB)",
                                    totalDownloaded / AS_MB, totalToDownload / AS_MB);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (5 * scaleFactor));
                        ImGui::ProgressBar(
                            (static_cast<float>(totalDownloaded) / static_cast<float>(totalToDownload)) * 1.0f,
                            ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f)
                        );
                    }

                    break;
                case DownloadAndInstallStep::DownloadSucceeded:

                    instStep = DownloadAndInstallStep::PrepareInstall;

                    spdlog::info("Download finished successfully");

                    break;
                case DownloadAndInstallStep::DownloadFailed:

                    isCancelDisabled = false;
                    isBackDisabled = false;
                    status = NV_E_DOWNLOAD_FAILED;

                    if (statusCode < CURL_LAST)
                    {
                        const auto curlCode = magic_enum::enum_name<CURLcode>(static_cast<CURLcode>(statusCode));
                        ImGui::Text(ICON_FK_EXCLAMATION_TRIANGLE " Download failed, cURL error: %s", curlCode.data());
                    }
                    else
                    {
                        ImGui::Text(ICON_FK_EXCLAMATION_TRIANGLE " Download failed, HTTP error: %s",
                                    httplib::status_message(statusCode));
                    }

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35 * scaleFactor));
                    if (ImGui::Button("Retry now"))
                    {
                        instStep = DownloadAndInstallStep::Begin;
                        currentPage = WizardPage::DownloadAndInstall;
                    }

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                    ImGui::Text("You can also press the " ICON_FK_ARROW_LEFT " button in the top left to retry.");

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                    ImGui::Text("Press the 'Cancel' button to abort and close.");

                    break;
                case DownloadAndInstallStep::PrepareInstall:
                    {
                        cfg.InvokeSetupAsync();

                        spdlog::debug("Setup process launched asynchronously");
                        instStep = DownloadAndInstallStep::InstallRunning;

                        break;
                    }
                case DownloadAndInstallStep::InstallRunning:
                    {
                        bool isRunning = false;
                        bool hasSetupFinished = false;
                        bool hasSucceeded = false;
                        DWORD exitCode = 0;
                        DWORD win32Error = 0;

                        if (cfg.GetSetupStatus(isRunning, hasSetupFinished, hasSucceeded, exitCode, win32Error))
                        {
                            if (isRunning)
                            {
                                ImGui::Text("Installing...");
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (5 * scaleFactor));
                                ui::IndeterminateProgressBar(
                                    ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
                            }
                            else if (hasSetupFinished)
                            {
                                spdlog::info("Setup finished; succeeded: {}, exitCode: {}, win32Error: {}",
                                             hasSucceeded, exitCode, win32Error);

                                if (win32Error == ERROR_SUCCESS && winapi::IsMsiExecErrorCode(exitCode))
                                {
                                    spdlog::error("msiexec failed with {} ({})",
                                                  winapi::GetLastErrorStdStr(exitCode), exitCode);

                                    lastExitCode = exitCode;
                                }

                                SetLastError(win32Error);

                                instStep = hasSucceeded
                                               ? DownloadAndInstallStep::InstallSucceeded
                                               : DownloadAndInstallStep::InstallFailed;
                            }
                        }
                        else
                        {
                            instStep = DownloadAndInstallStep::InstallFailed;
                        }
                    }

                    break;
                case DownloadAndInstallStep::InstallFailed:

                    std::call_once(errorUrlTriggered, [&cfg]()
                    {
                        if (!cfg.GetInstallErrorUrl().has_value())
                        {
                            return;
                        }

                        ShellExecuteA(
                            nullptr,
                            "open",
                            cfg.GetInstallErrorUrl().value().c_str(),
                            nullptr,
                            nullptr,
                            SW_SHOWNORMAL
                        );
                    });

                    if (GetLastError() == ERROR_SUCCESS)
                    {
                        ImGui::Text(
                            ICON_FK_EXCLAMATION_TRIANGLE
                            " Error! Installation failed with an unexpected exit code (%lu).",
                            lastExitCode
                        );

                        if (winapi::IsMsiExecErrorCode(lastExitCode))
                        {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                            ImGui::TextWrapped("Setup engine error: %s",
                                               winapi::GetLastErrorStdStr(lastExitCode).c_str());
                        }
                        else
                        {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                            ImGui::Text("Setup exit code: %lu", lastExitCode);
                        }
                    }
                    else
                    {
                        ImGui::Text(ICON_FK_EXCLAMATION_TRIANGLE " Error! Installation failed, error %s.",
                                    winapi::GetLastErrorStdStr().c_str());
                    }

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35 * scaleFactor));
                    /* TODO: currently bugged, fix later!
                    if (ImGui::Button("Retry now"))
                    {
                        instStep = DownloadAndInstallStep::PrepareInstall;
                    }

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                    */
                    ImGui::Text("You can also press the " ICON_FK_ARROW_LEFT " button in the top left to retry.");

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                    ImGui::Text("Press the 'Cancel' button to abort and close.");

                    isCancelDisabled = false;
                    isBackDisabled = false;
                    status = NV_E_SETUP_FAILED;

                // TODO: handle error

                    break;
                case DownloadAndInstallStep::InstallSucceeded:

                    //ImGui::Text("Done!");

                    // TODO: implement me, right now it simply exits

                    status = NV_S_UPDATE_FINISHED;
                    ++currentPage;

                    break;
                }

                ImGui::Unindent(leftBorderIndent);

                break;
            }
        case WizardPage::Finish:
            {
                window.close();

                break;
            }
        }

        ImGui::SetCursorPosY(mainViewport->WorkSize.y - (52 * scaleFactor));
        ImGui::Separator();

        ImGui::SetCursorPos(ImVec2(mainViewport->WorkSize.x - (70 * scaleFactor), navigateButtonOffsetY));
        ImGui::BeginDisabled(isCancelDisabled);
        if (ImGui::Button(currentPage == WizardPage::Finish ? "Finish" : "Cancel"))
        {
            window.close();
        }
        ImGui::EndDisabled();

        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    return static_cast<int>(status);
}
