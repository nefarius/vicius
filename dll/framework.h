// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS 1
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <fstream>
#include <algorithm>
#include <random>
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
#include <locale>

#include <argh.h>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <magic_enum/magic_enum.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
