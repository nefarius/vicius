#include "Updater.h"
#include "WizardPage.h"

#include <winhttp.h>


//
// Models
// 
#include "InstanceConfig.hpp"


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

#pragma region CLI parsing

	argh::parser cmdl;

	if (!util::ParseCommandLineArguments(cmdl))
	{
		// TODO: better fallback action?
		return EXIT_FAILURE;
	}

#pragma endregion

#pragma region CLI processing

	// TODO: implement me

#pragma endregion

	// updater configuration, defaults and app state
	models::InstanceConfig cfg(hInstance);

	if (!cfg.RequestUpdateInfo())
	{
		// TODO: add fallback actions
		return ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
	}

	if (cfg.IsNewerUpdaterAvailable())
	{
		// TODO: implement self-updating logic here
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
	bool isBackDisabled = false;
	bool isCancelDisabled = false;
	int selectedReleaseId;

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
		case WizardPage::SingleVersionSummary:
			isBackDisabled = false;

			ImGui::Indent(leftBorderIndent);
			ImGui::PushFont(G_Font_H1);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
			ImGui::Text("Update Summary");
			ImGui::PopFont();

			{
				const auto& release = cfg.GetLatestRelease();
				selectedReleaseId = 0;
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
			}

			ImGui::Unindent(leftBorderIndent);

			break;
		case WizardPage::MultipleVersionsOverview:
			isBackDisabled = false;

		// TODO: implement me
			break;
		case WizardPage::DownloadAndInstall:
			{
				isBackDisabled = true;
				isCancelDisabled = true;

				ImGui::Indent(leftBorderIndent);
				ImGui::PushFont(G_Font_H1);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
				ImGui::Text("Installing Updates");
				ImGui::PopFont();

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);

				// TODO: implement me


				static double totalToDownload = 0;
				static double totalDownloaded = 0;

				bool isDownloading = false;
				bool hasFinished = false;
				int statusCode = -1;

				// checks if a download is currently running or has been invoked
				if (!cfg.GetReleaseDownloadStatus(isDownloading, hasFinished, statusCode))
				{
					totalToDownload = 0;
					totalDownloaded = 0;

					// start download
					cfg.DownloadRelease(
						selectedReleaseId,
						[](void* pData, double downloadTotal, double downloaded, double uploadTotal,
						   double uploaded) -> int
						{
							UNREFERENCED_PARAMETER(pData);
							UNREFERENCED_PARAMETER(uploadTotal);
							UNREFERENCED_PARAMETER(uploaded);

							totalToDownload = downloadTotal;
							totalDownloaded = downloaded;
							return 0;
						});
				}

				if (isDownloading)
				{
					ImGui::Text("Downloading (%.2f MB of %.2f MB)",
					            totalDownloaded / AS_MB, totalToDownload / AS_MB);
					ImGui::ProgressBar(
						(static_cast<float>(totalDownloaded) / static_cast<float>(totalToDownload)) * 1.0f,
						ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f)
					);
				}

				if (hasFinished && statusCode == 200)
				{
					ImGui::Text("Installing...");
					ui::IndeterminateProgressBar(ImVec2(ImGui::GetContentRegionAvail().x - leftBorderIndent, 0.0f));

					// TODO: implement me
				}
				else if (hasFinished)
				{
					ImGui::Text("Error! Code: %d", statusCode);

					// TODO: implement me
				}

				/*
				static bool setupHasLaunched = false;
				if (hasFinished && !setupHasLaunched)
				{
					const auto tempFile = cfg.GetLocalReleaseTempFilePath();
					STARTUPINFOA info = {sizeof(info)};
					PROCESS_INFORMATION processInfo;
	
					// TODO: make non-blocking
	
					if (CreateProcessA(
						tempFile.string().c_str(),
						nullptr,
						nullptr,
						nullptr,
						TRUE,
						0,
						nullptr,
						nullptr,
						&info,
						&processInfo
					))
					{
						setupHasLaunched = true;
						WaitForSingleObject(processInfo.hProcess, INFINITE);
						CloseHandle(processInfo.hProcess);
						CloseHandle(processInfo.hThread);
					}
				}
				*/

				ImGui::Unindent(leftBorderIndent);

				break;
			}
		case WizardPage::End:
			// TODO: implement me
			break;
		}

		ImGui::SetCursorPosY(460);
		ImGui::Separator();

		ImGui::SetCursorPos(ImVec2(570, navigateButtonOffsetY));
		ImGui::BeginDisabled(isCancelDisabled);
		if (ImGui::Button(currentPage == WizardPage::End ? "Finish" : "Cancel"))
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

	return 0;
}
