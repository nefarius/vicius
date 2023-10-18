#pragma once

/*
 * Customize this file to tune the build to your liking
 */

//
// Regex for file name extraction, assumes "manufacturer_product_Updater" format
// Does NOT include file extension
// 
#define NV_FILENAME_REGEX       R"(^(\w+)_(\w+)_Updater$)"

//
// URL template (or absolute URL) where to find the update information
// 
#define NV_API_URL_TEMPLATE     "https://aiu.api.nefarius.systems/api/github/{}/updates?asJson=true"

//
// Default window title (displayed in taskbar)
// 
#define NV_WINDOW_TITLE         "Nefarius' Updater"

//
// Default fallback product name
// 
#define NV_PRODUCT_NAME         "Updater"

//
// Default logger name
// 
#define NV_LOGGER_NAME          "vicius-updater"

//
// Maximum allowed characters for URLs
// 
#define NV_API_URL_MAX_CHARS    2000
