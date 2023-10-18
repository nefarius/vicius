#include "pch.h"
#include "Common.h"
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

	cmdl.add_params({NV_CLI_LOG_LEVEL});

	if (!util::ParseCommandLineArguments(cmdl))
	{
		// TODO: better fallback action?
		spdlog::critical("Failed to parse command line arguments");
		return NV_E_CLI_PARSING;
	}

	// updater configuration, defaults and app state
	models::InstanceConfig cfg(hInstance, cmdl);

	// actions to perform when install is instructed
	if (cmdl[{NV_CLI_INSTALL}])
	{
		if (const auto autoRet = cfg.RegisterAutostart(); !autoRet)
		{
			// TODO: better fallback action?
			spdlog::critical("Failed to register in autostart");
			return NV_E_AUTOSTART;
		}

		if (const auto taskRet = cfg.CreateScheduledTask(); FAILED(std::get<0>(taskRet)))
		{
			// TODO: better fallback action?
			spdlog::critical("Failed to (re-)create scheduled task, error: {}", std::get<1>(taskRet));
			cfg.TryDisplayErrorDialog("Failed to create scheduled task", std::get<1>(taskRet));
			return NV_E_SCHEDULED_TASK;
		}

		if (const auto extRet = cfg.ExtractSelfUpdater(); !std::get<0>(extRet))
		{
			// TODO: better fallback action?
			spdlog::critical("Failed to extract self-updater, error: {}", std::get<1>(extRet));
			cfg.TryDisplayErrorDialog("Failed to extract self-updater", std::get<1>(extRet));
			return NV_E_EXTRACT_SELF_UPDATE;
		}

		spdlog::info("Installation tasks finished successfully");
		return EXIT_SUCCESS;
	}

	// actions to perform when running in autostart
	if (cmdl[{NV_CLI_AUTOSTART}])
	{
		if (const auto ret = cfg.CreateScheduledTask(); FAILED(std::get<0>(ret)))
		{
			spdlog::error("Failed to (re-)create Scheduled Task, error: {}", std::get<1>(ret));
			cfg.TryDisplayErrorDialog("Failed to create scheduled task", std::get<1>(ret));
			// TODO: anything else we can do in this case?
		}
	}

	// uninstall tasks
	if (cmdl[{NV_CLI_UNINSTALL}])
	{
		if (const auto autoRet = cfg.RemoveAutostart(); !autoRet)
		{
			// TODO: better fallback action?
			spdlog::critical("Failed to de-register in autostart");
			return NV_E_AUTOSTART;
		}

		if (const auto taskRet = cfg.RemoveScheduledTask(); FAILED(std::get<0>(taskRet)))
		{
			// TODO: better fallback action?
			spdlog::critical("Failed to delete scheduled task, error: {}", std::get<1>(taskRet));
			cfg.TryDisplayErrorDialog("Failed to delete scheduled task", std::get<1>(taskRet));
			return NV_E_SCHEDULED_TASK;
		}
	}

	// contact update server and get latest state and config
	if (!cfg.RequestUpdateInfo())
	{
		// TODO: add fallback actions
		spdlog::critical("Failed to get server response");
		return ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
	}

	// check for updater updates - updateception :D
	if (!cmdl[{NV_CLI_SKIP_SELF_UPDATE}] && cfg.IsNewerUpdaterAvailable())
	{
		spdlog::debug("Newer updater version available, invoking self-update");

		if (cfg.RunSelfUpdater())
		{
			return EXIT_SUCCESS;
		}

		spdlog::error("Failed to invoke self-update");
	}

	bool isOutdated = false;
	// run local product detection
	if (!cfg.IsInstalledVersionOutdated(isOutdated))
	{
		// TODO: add error handling
		spdlog::critical("Failed to detect installed product version");
		return ERROR_NO_DATA_DETECTED;
	}

	// we're up2date and silent, exit
	if (!isOutdated)
	{
		spdlog::info("Installed software is up-to-date");
		cfg.TryDisplayUpToDateDialog();
		return ERROR_SUCCESS;
	}
	
	constexpr int windowWidth = 640, windowHeight = 512;
	sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), cfg.GetTaskBarTitle(), sf::Style::None);

	window.setFramerateLimit(60);
	ImGui::SFML::Init(window, false);

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ui::LoadFonts(hInstance);
	ui::ApplyImGuiStyleDark();

	// Set window icon
	if (auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN)))
	{
		SendMessage(window.getSystemHandle(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
	}

	auto currentPage = WizardPage::Start;
	auto instStep = DownloadAndInstallStep::Begin;
	bool isBackDisabled = false;
	bool isCancelDisabled = false;
	STARTUPINFOA info = {sizeof(STARTUPINFOA)};
	PROCESS_INFORMATION updateProcessInfo{};
	DWORD status = ERROR_SUCCESS;

	sf::Vector2i grabbedOffset;
	auto grabbedWindow = false;
	sf::Clock deltaClock;
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			ImGui::SFML::ProcessEvent(window, event);

			if (event.type == sf::Event::Closed)
			{
				window.close();
			}
			// Mouse events used to react to dragging
			else if (event.type == sf::Event::MouseButtonPressed)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
				{
					grabbedOffset = window.getPosition() - sf::Mouse::getPosition();
					grabbedWindow = true;
				}
			}
			// Mouse events used to react to dragging
			else if (event.type == sf::Event::MouseButtonReleased)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
				{
					grabbedWindow = false;
				}
			}
			// Mouse events used to react to dragging
			else if (event.type == sf::Event::MouseMoved)
			{
				const auto offset = sf::Mouse::getPosition() - window.getPosition();
				// fake a titlebar and only drag when cursor is in that area
				if (grabbedWindow && offset.y < 30)
				{
					window.setPosition(sf::Mouse::getPosition() + grabbedOffset);
				}
			}
		}

		ImGui::SFML::Update(window, deltaClock.restart());

		//ImGui::ShowDemoWindow();

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar;

		// fakes a little window border/margin
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x + 5, mainViewport->WorkPos.y + 5));
		ImGui::SetNextWindowSize(ImVec2(windowWidth - 10, windowHeight - 10));

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

		float navigateButtonOffsetY = 470.0;
		float leftBorderIndent = 40.0;

		switch (currentPage)
		{
		case WizardPage::Start:
			{
				ImGui::Indent(leftBorderIndent);
				ImGui::PushFont(G_Font_H1);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
				ImGui::Text("Updates for %s are available", cfg.GetProductName().c_str());
				ImGui::PopFont();

				ImGui::Indent(leftBorderIndent);
				ImGui::PushFont(G_Font_H2);

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
				if (ImGui::Button(ICON_FK_DOWNLOAD " Download and install now"))
				{
					currentPage = cfg.HasSingleRelease()
						              ? WizardPage::SingleVersionSummary
						              : WizardPage::MultipleVersionsOverview;
				}

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
				if (ImGui::Button(ICON_FK_CLOCK_O " Remind me tomorrow"))
				{
					// TODO: implement me
					window.close();
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
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
				ImGui::Text("Update Summary");
				ImGui::PopFont();

				const auto& release = cfg.GetSelectedRelease();
				ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
				ImGui::BeginChild(
					"Summary",
					ImVec2(ImGui::GetContentRegionAvail().x, 360),
					false,
					windowFlags
				);
				markdown::RenderChangelog(release.summary);
				ImGui::EndChild();

				ImGui::SetCursorPos(ImVec2(530, navigateButtonOffsetY));
				if (ImGui::Button("Next"))
				{
					currentPage = WizardPage::DownloadAndInstall;
				}


				ImGui::Unindent(leftBorderIndent);

				break;
			}
		case WizardPage::MultipleVersionsOverview:
			{
				isBackDisabled = false;

				// TODO: implement me
				break;
			}
		case WizardPage::DownloadAndInstall:
			{
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
				}

				ImGui::Indent(leftBorderIndent);
				ImGui::PushFont(G_Font_H1);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
				ImGui::Text("Installing Updates");
				ImGui::PopFont();

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);

				bool isDownloading = false;
				bool hasFinished = false;
				int statusCode = -1;

				// checks if a download is currently running or has been invoked
				if (!cfg.GetReleaseDownloadStatus(isDownloading, hasFinished, statusCode))
				{
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
					instStep = statusCode == 200
						           ? DownloadAndInstallStep::DownloadSucceeded
						           : DownloadAndInstallStep::DownloadFailed;
				}

				switch (instStep)
				{
				case DownloadAndInstallStep::Downloading:

					ImGui::Text("Downloading (%.2f MB of %.2f MB)",
					            totalDownloaded / AS_MB, totalToDownload / AS_MB);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
					ImGui::ProgressBar(
						(static_cast<float>(totalDownloaded) / static_cast<float>(totalToDownload)) * 1.0f,
						ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f)
					);

					break;
				case DownloadAndInstallStep::DownloadSucceeded:

					instStep = DownloadAndInstallStep::PrepareInstall;

					break;
				case DownloadAndInstallStep::DownloadFailed:

					ImGui::Text("Error! Code: %d", statusCode);

				// TODO: implement me

					break;
				case DownloadAndInstallStep::PrepareInstall:
					{
						const auto& release = cfg.GetSelectedRelease();

						if (const auto tempFile = cfg.GetLocalReleaseTempFilePath(); !CreateProcessA(
							tempFile.string().c_str(),
							const_cast<LPSTR>(release.launchArguments.c_str()),
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
							spdlog::error("Failed to launch {}, error {}",
							              tempFile.string(), GetLastError());
							instStep = DownloadAndInstallStep::InstallLaunchFailed;
						}
						else
						{
							spdlog::debug("Setup process launched successfully");
							instStep = DownloadAndInstallStep::InstallRunning;
						}

						break;
					}
				case DownloadAndInstallStep::InstallLaunchFailed:

					ImGui::Text("Error! Failed to launch setup");

				// TODO: handle error

					break;
				case DownloadAndInstallStep::InstallRunning:

					if (auto waitResult = WaitForSingleObject(updateProcessInfo.hProcess, 1); waitResult ==
						WAIT_TIMEOUT)
					{
						ImGui::Text("Installing...");
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
						ui::IndeterminateProgressBar(ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));
					}
					else if (waitResult == WAIT_OBJECT_0)
					{
						DWORD exitCode = 0;

						GetExitCodeProcess(updateProcessInfo.hProcess, &exitCode);

						CloseHandle(updateProcessInfo.hProcess);
						CloseHandle(updateProcessInfo.hThread);
						RtlZeroMemory(&updateProcessInfo, sizeof(updateProcessInfo));

						if (DeleteFileA(cfg.GetLocalReleaseTempFilePath().string().c_str()) == 0)
						{
							spdlog::warn("Failed to delete temporary file {}, error {}",
							             cfg.GetLocalReleaseTempFilePath().string(), GetLastError());
						}

						const auto& release = cfg.GetSelectedRelease();

						if (release.exitCode.skipCheck)
						{
							spdlog::debug("Skipping error code check as per configuration");
							instStep = DownloadAndInstallStep::InstallSucceeded;
							break;
						}

						if (std::ranges::find(release.exitCode.successCodes, exitCode) != release.exitCode.successCodes.
							end())
						{
							spdlog::debug("Exit code {} marked as success-condition", exitCode);
							instStep = DownloadAndInstallStep::InstallSucceeded;
							break;
						}

						instStep = DownloadAndInstallStep::InstallFailed;
					}

					break;
				case DownloadAndInstallStep::InstallFailed:

					ImGui::Text("Error! Installation failed");

				// TODO: handle error

					break;
				case DownloadAndInstallStep::InstallSucceeded:

					//ImGui::Text("Done!");

					// TODO: implement me

					status = ERROR_SUCCESS;
					++currentPage;

					break;
				}

				ImGui::Unindent(leftBorderIndent);

				break;
			}
		case WizardPage::Finish:
			{
				// TODO: implement me

				window.close();

				break;
			}
		}

		ImGui::SetCursorPosY(460);
		ImGui::Separator();

		ImGui::SetCursorPos(ImVec2(570, navigateButtonOffsetY));
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
