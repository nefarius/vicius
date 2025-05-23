#pragma once

//
// WinAPI
//
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS 1
#include <windows.h>
#include <tchar.h>
#include <Dwmapi.h>
#include <shellapi.h>
#include <winhttp.h>
#include <comdef.h>
#include <ole2.h>
#include <taskschd.h>
#include <wincrypt.h>
#include <d3d11.h>

//
// ImGui, Fonts
//
#if 0
#include "ImGuiConfig.h"
#define IMGUI_USER_CONFIG "ImGuiConfig.h"
#endif
#include "imgui.h"
#include "imgui_freetype.h"
#include "IconsForkAwesome.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

//
// Utility packages
//
#include "argh.h"
#include <semver/semver.hpp>
#include <magic_enum/magic_enum.hpp>
#include <winreg/WinReg.hpp>
#include <hash-library/md5.h>
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>
#include <scope_guard.hpp>
#include <nlohmann/json.hpp>
#include <httplib.h>

//
// Logging
//
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

//
// STL
//
#include <filesystem>
#include <fstream>
#include <tuple>
#include <random>
#include <algorithm>
#include <locale>
#include <regex>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include <expected>
#include <variant>
#include <format>
#include <chrono>

//
// neflib
// 
#include <nefarius/neflib/MiscWinApi.hpp>

//
// Custom
//
#include "UniUtil.h"
#include "resource.h"
#include "CustomizeMe.h"
#include "Crypto.h"
#include "UI.h"
#include "Formatters.h"
