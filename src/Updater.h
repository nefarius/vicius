#pragma once

// WinAPI
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Dwmapi.h>
#include <shellapi.h>

// ImGui, Fonts
#include "imgui.h"
#include "imgui-SFML.h"
#include "imgui_markdown.h"
#include "IconsFontAwesome5.h"

// SFML
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

//
// STL
// 
#include <format>

// CLI parser
#include <argh.h>

//
// Custom
// 
#include "UniUtil.h"
#include "resource.h"


void RenderChangelog(const std::string& markdown);
void ApplyImGuiStyleDark();
void LoadFonts(HINSTANCE hInstance, const float sizePixels = 16.0f);
