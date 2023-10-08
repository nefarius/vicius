#include "Updater.h"

//
// STL
// 
#include <format>
#include <regex>

#include <restclient-cpp/restclient.h>


//
// Enable visual styles
// 
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(szCmdLine);
	UNREFERENCED_PARAMETER(iCmdShow);

#pragma region CLI parsing

	argh::parser cmdl;

	LPWSTR* szArglist;
	int nArgs;
	int i;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (nullptr == szArglist)
	{
		return EXIT_FAILURE;
	}

	std::vector<const char*> argv;
	std::vector<std::string> narrow;

	for (i = 0; i < nArgs; i++)
	{
		narrow.push_back(ConvertWideToANSI(std::wstring(szArglist[i])));
	}

	argv.resize(nArgs);
	std::ranges::transform(narrow, argv.begin(), [](const std::string& arg) { return arg.c_str(); });

	argv.push_back(nullptr);

	cmdl.parse(nArgs, &argv[0]);

#pragma endregion

#pragma region CLI processing

	// TODO: implement me

#pragma endregion

#pragma region Filename analysis

	//
	// See if we can parse the product name from the process file name
	// 

	auto appPath = GetImageBasePathW();
	auto fileName = appPath.stem().string();

	std::regex product_regex(NV_FILENAME_REGEX, std::regex_constants::icase);
	auto matches_begin = std::sregex_iterator(fileName.begin(), fileName.end(), product_regex);
	auto matches_end = std::sregex_iterator();

	std::string username;
	std::string repository;

	if (matches_begin != matches_end)
	{
		if (const std::smatch& match = *matches_begin; match.size() == 3)
		{
			username = match[1];
			repository = match[2];
		}
	}

	if (!username.empty() && !repository.empty())
	{
		auto url = std::format(NV_API_URL_TEMPLATE, username, repository);
		RestClient::Response r = RestClient::get(url);
	}

#pragma endregion

	const int windowWidth = 640, windowHeight = 512;
	sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), NV_WINDOW_TITLE, sf::Style::Titlebar);

	window.setFramerateLimit(60);
	ImGui::SFML::Init(window, false);

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	LoadFonts(hInstance);
	//ApplyImGuiStyleDark();

	// Set window icon
	if (auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN)))
	{
		SendMessage(window.getSystemHandle(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
	}

	// Enable dark titlebar
	int32_t preference = 1;
	DwmSetWindowAttribute(window.getSystemHandle(), DWMWA_USE_IMMERSIVE_DARK_MODE, &preference, sizeof(int32_t));

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

		bool open = true;
		// fakes a little window border/margin
		const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 5, main_viewport->WorkPos.y + 5));
		ImGui::SetNextWindowSize(ImVec2(windowWidth - 10, windowHeight - 10));
		ImGui::Begin("Main", &open,
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar);

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

		ImGui::End();

		window.clear();
		ImGui::SFML::Render(window);
		window.display();
	}

	ImGui::SFML::Shutdown();

	return 0;
}
