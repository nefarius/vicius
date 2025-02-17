#include "pch.h"
#include "Common.h"
#include "Util.h"
#include "WizardPage.h"
#include "InstanceConfig.hpp"
#include "DownloadAndInstall.hpp"


//
// Enable visual styles
//
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#define AS_MB (1024 * 1024)

extern ImFont* G_Font_H1;
extern ImFont* G_Font_H2;
extern ImFont* G_Font_H3;

// Data
ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(szCmdLine);

    argh::parser cmdl;

    cmdl.add_params({
        NV_CLI_PARAM_LOG_LEVEL,
        NV_CLI_PARAM_LOG_TO_FILE,
        NV_CLI_PARAM_CHANNEL,
        NV_CLI_PARAM_ADD_HEADER,
        NV_CLI_PARAM_OVERRIDE_OK,
        NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE,
        NV_CLI_PARAM_LOCAL_VERSION,
        NV_CLI_PARAM_FORCE_LOCAL_VERSION
    });

#if !defined(NDEBUG)
    cmdl.add_params(NV_CLI_PARAM_SERVER_URL);
#endif

    if (!util::ParseCommandLineArguments(cmdl))
    {
        // TODO: better fallback action?
        spdlog::critical("Failed to parse command line arguments");
        return NV_E_CLI_PARSING;
    }

    DWORD earlyAbortCode = 0;

    // updater configuration, defaults and app state
    models::InstanceConfig cfg(hInstance, cmdl, &earlyAbortCode);

    if (earlyAbortCode == NV_E_INVALID_MODULE_NAME)
    {
        spdlog::critical("Invalid module name detected");
        cfg.TryDisplayErrorDialog(
            "Invalid updater file name detected",
            "The updater file name contains some invalid sequences, "
            "make sure you have 'Hide extensions for known file types' "
            "turned OFF in Windows Explorer, fix the file name and try again."
            );
        return (int)earlyAbortCode;
    }

    if (earlyAbortCode)
    {
        spdlog::critical("Instance initialization failed");
        cfg.TryDisplayErrorDialog(
            "Instance initialization failed",
            std::format("Booting the updater failed with error code {}", earlyAbortCode)
            );
        return (int)earlyAbortCode;
    }

#pragma region Install command

    // actions to perform when install is instructed
#if !defined(NV_FLAGS_ALWAYS_RUN_INSTALL)
    if (!cmdl[ {NV_CLI_TEMPORARY} ] && cmdl[ {NV_CLI_INSTALL} ])
    {
#endif
        if (!cmdl[ {NV_CLI_NO_AUTOSTART} ])
        {
            if (const auto autoRet = cfg.RegisterAutostart(); !std::get<0>(autoRet))
            {
                // TODO: better fallback action?

                spdlog::critical("Failed to register in autostart");
                cfg.TryDisplayErrorDialog("Failed to register in autostart", std::get<1>(autoRet));
                return NV_E_AUTOSTART;
            }
        }

        if (!cmdl[ {NV_CLI_NO_SCHEDULED_TASK} ])
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

        return cfg.GetSuccessExitCode(NV_S_INSTALL);
    }
#endif

#pragma endregion

#pragma region Autostart tasks

    // actions to perform when running in autostart
    if (!cmdl[ {NV_CLI_TEMPORARY} ] && cmdl[ {NV_CLI_AUTOSTART} ])
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
    if (!cmdl[ {NV_CLI_TEMPORARY} ] && cmdl[ {NV_CLI_UNINSTALL} ])
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

        return cfg.GetSuccessExitCode(NV_S_INSTALL);
    }

#pragma endregion

    // purge postpone data, if any
    if (cmdl[ {NV_CLI_PURGE_POSTPONE} ])
    {
        int successCode = cfg.GetSuccessExitCode(NV_S_POSTPONE_PURGE);
        return cfg.PurgePostponeData() ? successCode : NV_E_POSTPONE_PURGE_FAILED;
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
    if (!cmdl[ {NV_CLI_TEMPORARY} ] && (!cmdl[ {NV_CLI_SKIP_SELF_UPDATE} ] && cfg.IsNewerUpdaterAvailable()))
    {
        spdlog::debug("Newer updater version available, invoking self-update");

        if (cfg.RunSelfUpdater())
        {
            return cfg.GetSuccessExitCode(NV_S_SELF_UPDATER);
        }

        spdlog::error("Failed to invoke self-update");
    }

    // restart ourselves from a temporary location, if requested
    if (cfg.TryRunTemporaryProcess())
    {
        return cfg.GetSuccessExitCode(NV_S_LAUNCHED_TEMPORARY);
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
        return cfg.GetSuccessExitCode(NV_S_UP_TO_DATE);
    }

    // there's a pending update but user chose to postpone
    if (cfg.IsInPostponePeriod())
    {
        spdlog::info("Postpone period active, exiting");
        return cfg.GetSuccessExitCode(NV_S_POSTPONE_PERIOD);
    }

#pragma region Busy state check

    // check if we are currently bothering the user
    if (!cmdl[ {NV_CLI_IGNORE_BUSY_STATE} ] && cfg.IsSilent())
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

#pragma endregion

    //
    // Main GUI creation
    //

    constexpr int windowWidth = 640, windowHeight = 512;

    const auto windowTitle = nefarius::utilities::ConvertToWide(cfg.GetWindowTitle());

    // Create application window
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L, 0L,
        hInstance,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN)), nullptr, nullptr, nullptr,
        L"NefariusViciusUpdaterClass",
        nullptr
    };
    ::RegisterClassExW(&wc);
    const HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
        nullptr, nullptr, wc.hInstance, nullptr
        );

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return NV_E_CREATE_D3D_DEVICE;
    }

    cfg.SetWindowHandle(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // disable unused features
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

#define SCALED(_val_)   ((_val_) * scaleFactor)

#pragma warning(disable : 4244)

    // get DPI scale
    auto dpi = winapi::GetWindowDPI(hwnd);
    float scaleFactor = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI;
    auto scaledWidth = SCALED(windowWidth);
    auto scaledHeight = SCALED(windowHeight);

    // Get the screen width and height
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);  // Screen width
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // Screen height

    // Calculate the window's position to center it
    int xPos = (screenWidth - scaledWidth) / 2;
    int yPos = (screenHeight - scaledHeight) / 2;

    ::SetWindowPos(
        hwnd,
        HWND_TOP,
        xPos, yPos,
        scaledWidth,
        scaledHeight,
        SWP_NOZORDER
        );
    io.DisplaySize = ImVec2(scaledWidth, scaledHeight);

#pragma warning(default : 4244)

    ui::LoadFonts(hInstance, 16, scaleFactor);
    ui::ApplyImGuiStyleDark(scaleFactor);

    winapi::SetDarkMode(hwnd);

    // Show the window
    ::ShowWindow(hwnd, iCmdShow);
    ::UpdateWindow(hwnd);

    auto& colors = ImGui::GetStyle().Colors;
    ImVec4 clear_color = colors[ ImGuiCol_WindowBg ];

    auto currentPage = WizardPage::Start;
    auto instStep = DownloadAndInstallStep::Begin;
    bool isBackDisabled = false;
    bool isCancelDisabled = false;
    DWORD status = ERROR_SUCCESS;
    std::once_flag errorUrlTriggered;
    std::stop_source stopSource;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

#pragma region Main ImGui content building

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x - 1, mainViewport->WorkPos.y - 1));
        ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight));

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

        float navigateButtonOffsetY = mainViewport->WorkSize.y - SCALED(42);
        float leftBorderIndent = 40.0;

        switch (currentPage)
        {
            case WizardPage::Start:
            {
                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(30));
                ImGui::Text("Updates for %s are available", cfg.GetProductName().c_str());
                ImGui::PopFont();

                ImGui::Indent(leftBorderIndent);
                ImGui::PushFont(G_Font_H2);

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(30));
                if (ImGui::Button(ICON_FK_DOWNLOAD " Display update details now"))
                {
                    currentPage = cfg.HasSingleRelease()
                                      ? WizardPage::SingleVersionSummary
                                      : WizardPage::MultipleVersionsOverview;
                }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(20));
                if (ImGui::Button(ICON_FK_CLOCK_O " Remind me tomorrow"))
                {
                    cfg.SetPostponeData();
                    status = cfg.GetSuccessExitCode(NV_S_USER_POSTPONED);
                    PostQuitMessage((int)status);
                }

                if (cfg.GetHelpUrl().has_value())
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(20));
                    if (ImGui::Button(ICON_FK_QUESTION " Open help web page"))
                    {
                        ShellExecuteA(
                            nullptr,
                            "open",
                            cfg.GetHelpUrl().value().c_str(),
                            nullptr, nullptr,
                            SW_SHOWNORMAL
                        );
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
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(30));
                ImGui::Text("Update Summary");
                ImGui::PopFont();

                const auto& release = cfg.GetSelectedRelease();
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
                ImGui::BeginChild("Summary",
                                  ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - SCALED(60)),
                                  false,
                                  windowFlags);
                markdown::RenderChangelog(release.summary.empty() ? "This release contains no summary." : release.summary);
                ImGui::EndChild();

                ImGui::SetCursorPos(ImVec2(mainViewport->WorkSize.x - SCALED(215), navigateButtonOffsetY));
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
                currentPage = WizardPage::SingleVersionSummary;
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
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(30));
                ImGui::Text("Installing Updates");
                ImGui::PopFont();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(30));

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
                        [](void* pData, double downloadTotal, double downloaded, double uploadTotal, double uploaded) -> int
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
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(5));
                            ui::IndeterminateProgressBar(ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
                        }
                        else
                        {
                            ImGui::Text("Downloading (%.2f MB of %.2f MB)", totalDownloaded / AS_MB, totalToDownload / AS_MB);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(5));
                            ImGui::ProgressBar((static_cast<float>(totalDownloaded) / static_cast<float>(totalToDownload)) * 1.0f,
                                               ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
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

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(35));
                        if (ImGui::Button("Retry now"))
                        {
                            instStep = DownloadAndInstallStep::Begin;
                            currentPage = WizardPage::DownloadAndInstall;
                        }

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(15));
                        ImGui::Text("You can also press the " ICON_FK_ARROW_LEFT " button in the top left to retry.");

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(15));
                        ImGui::Text("Press the 'Cancel' button to abort and close.");

                        break;
                    case DownloadAndInstallStep::PrepareInstall:
                    {
                        cfg.InvokeSetupAsync(stopSource.get_token());

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
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(5));
                                ui::IndeterminateProgressBar(ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
                            }
                            else if (hasSetupFinished)
                            {
                                spdlog::info("Setup finished; succeeded: {}, exitCode: {}, win32Error: {}", hasSucceeded,
                                             exitCode, win32Error);

                                if (win32Error == ERROR_SUCCESS && winapi::IsMsiExecErrorCode(exitCode))
                                {
                                    spdlog::error("msiexec failed with {} ({})", winapi::GetLastErrorStdStr(exitCode), exitCode);

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

                        std::call_once(errorUrlTriggered,
                                       [ &cfg ]()
                                       {
                                           if (!cfg.GetInstallErrorUrl().has_value())
                                           {
                                               return;
                                           }

                                           ShellExecuteA(nullptr, "open", cfg.GetInstallErrorUrl().value().c_str(), nullptr,
                                                         nullptr, SW_SHOWNORMAL);
                                       });

                        if (GetLastError() == ERROR_SUCCESS)
                        {
                            ImGui::Text(ICON_FK_EXCLAMATION_TRIANGLE
                                        " Error! Installation failed with an unexpected exit code (%lu).",
                                        lastExitCode);

                            if (winapi::IsMsiExecErrorCode(lastExitCode))
                            {
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(15));
                                ImGui::TextWrapped("Setup engine error: %s", winapi::GetLastErrorStdStr(lastExitCode).c_str());
                            }
                            else
                            {
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(15));
                                ImGui::Text("Setup exit code: %lu", lastExitCode);
                            }
                        }
                        else
                        {
                            ImGui::Text(ICON_FK_EXCLAMATION_TRIANGLE " Error! Installation failed, error %s.",
                                        winapi::GetLastErrorStdStr().c_str());
                        }

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(35));
                    /* TODO: currently bugged, fix later!
                if (ImGui::Button("Retry now"))
                {
                    instStep = DownloadAndInstallStep::PrepareInstall;
                }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15 * scaleFactor));
                */
                        ImGui::Text("You can also press the " ICON_FK_ARROW_LEFT " button in the top left to retry.");

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SCALED(15));
                        ImGui::Text("Press the 'Cancel' button to abort and close.");

                        isCancelDisabled = false;
                        isBackDisabled = false;
                        status = NV_E_SETUP_FAILED;

                    // TODO: handle error

                        break;
                    case DownloadAndInstallStep::InstallSucceeded:

                        //ImGui::Text("Done!");

                        // TODO: implement me, right now it simply exits

                        status = cfg.GetSuccessExitCode(NV_S_UPDATE_FINISHED);
                        ++currentPage;

                        break;
                }

                ImGui::Unindent(leftBorderIndent);

                break;
            }
            case WizardPage::Finish:
            {
                PostQuitMessage(status);

                break;
            }
        }

        ImGui::SetCursorPosY(mainViewport->WorkSize.y - SCALED(52));
        ImGui::Separator();

        ImGui::SetCursorPos(ImVec2(mainViewport->WorkSize.x - SCALED(70), navigateButtonOffsetY));
        ImGui::BeginDisabled(isCancelDisabled);
        if (ImGui::Button(currentPage == WizardPage::Finish ? "Finish" : "Cancel"))
        {
            PostQuitMessage(status);
        }
        ImGui::EndDisabled();

        ImGui::End();

#pragma endregion

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[ 4 ] = {
            clear_color.x * clear_color.w,
            clear_color.y * clear_color.w,
            clear_color.z * clear_color.w,
            clear_color.w
        };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0); // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return static_cast<int>(status);
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    constexpr D3D_FEATURE_LEVEL featureLevelArray[ 2 ] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
        );
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice, &featureLevel,
            &g_pd3dDeviceContext
            );
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
            g_ResizeHeight = (UINT)HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
