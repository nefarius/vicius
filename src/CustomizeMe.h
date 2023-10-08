#pragma once

/*
 * Customize this file to tune the build to your liking
 */

//
// Regex for file name extraction, assumes "username_repository_Updater" format
// 
#define NV_FILENAME_REGEX		R"(^(\w+)_(\w+)_Updater$)"

//
// Default window title
// 
#define NV_WINDOW_TITLE			"Nefarius' Updater"

#define NV_API_URL_MAX_CHARS	2000

#define NV_API_URL_TEMPLATE		"https://aiu.api.nefarius.systems/api/github/{}/updates?asJson=true"
