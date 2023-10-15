#pragma once

// 
// WinAPI
// 
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Dwmapi.h>
#include <shellapi.h>

// 
// ImGui, Fonts
// 
#include "imgui.h"
#include "imgui-SFML.h"
#include "imgui_markdown.h"
#include "IconsForkAwesome.h"

// 
// SFML
// 
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

//
// STL
// 
#include <filesystem>

// 
// CLI parser
// 
#include <argh.h>

//
// Util
// 
#include <neargye/semver.hpp>

//
// Logging
// 
#include <spdlog/spdlog.h>

// 
// Custom
// 
#include "UniUtil.h"
#include "resource.h"
#include "CustomizeMe.h"
#include "DownloadAndInstall.hpp"


#define NV_UPDATER_ADS_NAME		":updater.dll"


//
// Functions
// 

namespace markdown
{
	void RenderChangelog(const std::string& markdown);
}

namespace ui
{
	void ApplyImGuiStyleDark();
	void LoadFonts(HINSTANCE hInstance, const float sizePixels = 16.0f);
	void IndeterminateProgressBar(const ImVec2& size_arg);
}

namespace util
{
	std::filesystem::path GetImageBasePathW();
	semver::version GetVersionFromFile(const std::filesystem::path& filePath);
	bool ParseCommandLineArguments(argh::parser& cmdl);
	std::string trim(const std::string& str, const std::string& whitespace = " \t");
	bool icompare_pred(unsigned char a, unsigned char b);
	bool icompare(std::string const& a, std::string const& b);
	bool IsAdmin(int& errorCode);
}

namespace winapi
{
	DWORD IsAppRunningAsAdminMode(PBOOL IsAdmin);
}
