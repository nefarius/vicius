#pragma once

// 
// WinAPI
// 
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <Dwmapi.h>
#include <shellapi.h>
#include <winhttp.h>
#include <comdef.h>
#include <ole2.h>
#include <taskschd.h>

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
// Utility packages
// 
#include <argh.h>
#include <neargye/semver.hpp>
#include <magic_enum.hpp>
#include <winreg/WinReg.hpp>
#include <hash-library/md5.h>
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#include <scope_guard.hpp>

//
// STL
// 
#include <filesystem>
#include <tuple>
#include <random>
