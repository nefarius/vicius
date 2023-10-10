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

	LoadFonts(hInstance);
	ApplyImGuiStyleDark();

	// Set window icon
	if (auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN)))
	{
		SendMessage(window.getSystemHandle(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
	}

	auto currentPage = WizardPage::Start;

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
			ImGui::Text(ICON_FK_ARROW_LEFT);
		}
		else
		{
			if (ImGui::SmallButton(ICON_FK_ARROW_LEFT))
			{
				--currentPage;
			}
		}
		ImGui::SameLine();
		ImGui::Text("Found Updates for %s", cfg.GetProductName().c_str());

		switch (currentPage)
		{
		case WizardPage::Start:
			ImGui::Indent(40);
			ImGui::PushFont(G_Font_H1);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30);
			ImGui::Text("Updates for %s are available", cfg.GetProductName().c_str());
			ImGui::PopFont();

			ImGui::Indent(40);
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
			ImGui::Unindent(80);
			break;
		}

		ImGui::SetCursorPosY(460);
		ImGui::Separator();

		ImGui::SetCursorPos(ImVec2(570, 470));
		if (ImGui::Button(currentPage == WizardPage::End ? "Finish" : "Cancel"))
		{
			window.close();
		}

		/*
		// icons example
		ImGui::Button(ICON_FK_SEARCH " Search");
		ImGui::Button(ICON_FK_ARROW_LEFT);

		// example markdown
		const auto markdownText = u8R"(
# H1 Header: Text and Links
You can add [links like this one to enkisoftware](https://www.enkisoftware.com/) and lines will wrap well.
You can also insert images ![image alt text](image identifier e.g. filename)
Horizontal rules:
***
___
*Emphasis* and **strong emphasis** change the appearance of the text.
## H2 Header: indented text.
  This text has an indent (two leading spaces).
	This one has two.
### H3 Header: Lists
  * Unordered lists
	* Lists can be indented with two extra spaces.
  * Lists can have [links like this one to Avoyd](https://www.avoyd.com/) and *emphasized text*
)";

		RenderChangelog(std::string((const char*)markdownText));
		*/

		ImGui::End();

		window.clear();
		ImGui::SFML::Render(window);
		window.display();
	}

	ImGui::SFML::Shutdown();

	return 0;
}
