#pragma once

/*
 * Customize this file to tune the build to your liking
 */


//
// URL template (or absolute URL) where to find the update information
// The {} will be replaced with the "manufacturer/product" sub-path
// 
#define NV_API_URL_TEMPLATE     "https://aiu.api.nefarius.systems/api/github/{}/updates?asJson=true"

//
// Regex for file name extraction, assumes "manufacturer_product_Updater" format
// Does NOT include file extension
// 
#define NV_FILENAME_REGEX       R"(^(\w+)_(\w+)_Updater$)"

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

//
// Name of the custom HTTP headers added to each server request
// 
#define NV_HTTP_HEADERS_NAME    "Vicius"


/*
 * Compiler switches turning optional features on or off
 */


//
// Uncomment to build without local configuration JSON file support
// 
//#define NV_FLAGS_NO_CONFIG_FILE

//
// Uncomment to build without setting custom HTTP headers (X-Vicius-...)
// 
//#define NV_FLAGS_NO_VENDOR_HEADERS
