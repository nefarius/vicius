#pragma once

// 
// Constants
// 

#define NV_ADS_UPDATER_NAME         ":updater.dll"
#define NV_CLI_INSTALL              "--install"
#define NV_CLI_UNINSTALL            "--uninstall"
#define NV_CLI_AUTOSTART            "--autostart"
#define NV_CLI_BACKGROUND           "--background"
#define NV_CLI_PARAM_LOG_LEVEL      "--log-level"
#define NV_CLI_SKIP_SELF_UPDATE     "--skip-self-update"
#define NV_CLI_SILENT               "--silent"
#define NV_CLI_SILENT_UPDATE        "--silent-update"
#define NV_CLI_IGNORE_BUSY_STATE    "--ignore-busy-state"
#define NV_CLI_PARAM_LOG_TO_FILE    "--log-to-file"
#define NV_CLI_PARAM_SERVER_URL     "--server-url"
#define NV_CLI_NO_SCHEDULED_TASK    "--no-scheduled-task"
#define NV_CLI_NO_AUTOSTART         "--no-autostart"
#define NV_CLI_PARAM_CHANNEL        "--channel"
#define NV_CLI_PARAM_ADD_HEADER     "--add-header"
#define NV_CLI_PARAM_OVERRIDE_OK    "--override-success-code"

//
// App error exit codes
// 

#define NV_E_CLI_PARSING            100
#define NV_E_AUTOSTART              101
#define NV_E_SCHEDULED_TASK         102
#define NV_E_EXTRACT_SELF_UPDATE    103
#define NV_E_SERVER_RESPONSE        104
#define NV_E_PRODUCT_DETECTION      105
#define NV_E_BUSY                   106
#define NV_E_DOWNLOAD_FAILED        107
#define NV_E_SETUP_LAUNCH_FAILED    108
#define NV_E_SETUP_FAILED           109

#define NV_S_INSTALL                200
#define NV_S_SELF_UPDATER           201
#define NV_S_UP_TO_DATE             202
#define NV_S_UPDATE_FINISHED        203
#define NV_S_USER_POSTPONED         204
#define NV_S_POSTPONE_PERIOD        205


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
    void LoadFonts(HINSTANCE hInstance, float sizePixels = 16.0f);
    void IndeterminateProgressBar(const ImVec2& size_arg);
    void UpdateUIScaling(float scale = 1.0f);
}
