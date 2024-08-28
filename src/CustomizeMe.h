#pragma once

/*
 * Customize this file to tune the build to your liking
 */


#if __has_include("ViciusPreCustomizeMe.h")
#include "ViciusPreCustomizeMe.h"
#endif


//
// URL template (or absolute URL) where to find the update information
// The {} will be replaced with the "manufacturer/product(/channel)" sub-path
// CAUTION: set NV_FLAGS_NO_SERVER_URL_RESOURCE to enforce these values
// See also https://docs.nefarius.at/projects/Vicius/Server-Discovery/
//
#if defined(NDEBUG)
// this value will be used for RELEASE builds
#define NV_API_URL_TEMPLATE "https://vicius.api.nefarius.systems/api/{}/updates.json"
#else
// this value will be used for DEBUG builds
#define NV_API_URL_TEMPLATE "http://localhost:5200/api/{}/updates.json"
#endif

//
// Customize the updater's own version;
//
// for example, if you customize the updater, you probably want to be able to version
// your customizations, independently of the vicius version.
//
// In this case, you could create `v1.7.813+myname1` by specifying '+myname1' here.
//
#define NV_CUSTOM_VERSION_SUFFIX ""


//
// Regex for file name extraction, assumes "manufacturer_product_Updater" format
// Matching value does NOT include file extension
//
#define NV_FILENAME_REGEX    R"(^(\w+)_(\w+)_Updater.*?)"

//
// Default window title (displayed in taskbar)
//
#define NV_WINDOW_TITLE      "Nefarius' Updater"

//
// Default fallback product name (displayed in main UI window)
//
#define NV_PRODUCT_NAME      "Updater"

//
// Default logger name
//
#define NV_LOGGER_NAME       "vicius-updater"

//
// Maximum allowed characters for URLs
//
#define NV_API_URL_MAX_CHARS 2000

//
// Name of the custom HTTP headers added to each server request
//
#define NV_HTTP_HEADERS_NAME "Vicius"

//
// The default setup exit code assuming success, if not specified otherwise
//
#define NV_SUCCESS_EXIT_CODE 0


/*
 * Compiler switches turning optional features on or off
 */


//
// Uncomment to build without ever reading the server URL from the string table resource
// > If this is set
// >   https://docs.nefarius.at/projects/Vicius/Server-Discovery/#edit-the-string-table-resource
// > will no longer work
//
#define NV_FLAGS_NO_SERVER_URL_RESOURCE

//
// Uncomment to build without local configuration JSON file support
//
//#define NV_FLAGS_NO_CONFIG_FILE

//
// Uncomment to build without setting custom HTTP headers (X-Vicius-...)
//
//#define NV_FLAGS_NO_VENDOR_HEADERS

//
// Uncomment to build without any logging support whatsoever
//
//#define NV_FLAGS_NO_LOGGING

//
// Uncomment to always run install steps on launch
//
//#define NV_FLAGS_ALWAYS_RUN_INSTALL


#if __has_include("ViciusPostCustomizeMe.h")
#include "ViciusPostCustomizeMe.h"
#endif
