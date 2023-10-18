#pragma once

// 
// Constants
// 

#define NV_ADS_UPDATER_NAME         ":updater.dll"
#define NV_CLI_INSTALL              "--install"
#define NV_CLI_UNINSTALL            "--uninstall"
#define NV_CLI_AUTOSTART            "--autostart"
#define NV_CLI_BACKGROUND           "--background"
#define NV_CLI_LOG_LEVEL            "--log-level"
#define NV_CLI_SKIP_SELF_UPDATE     "--skip-self-update"
#define NV_CLI_SILENT               "--silent"

#define NV_E_CLI_PARSING            100
#define NV_E_AUTOSTART              101
#define NV_E_SCHEDULED_TASK         102
#define NV_E_EXTRACT_SELF_UPDATE    103


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
}

namespace util
{
    std::filesystem::path GetImageBasePathW();
    semver::version GetVersionFromFile(const std::filesystem::path& filePath);
    bool ParseCommandLineArguments(argh::parser& cmdl);
    std::string trim(const std::string& str, const std::string& whitespace = " \t");
    bool icompare_pred(unsigned char a, unsigned char b);
    bool icompare(const std::string& a, const std::string& b);
    bool IsAdmin(int& errorCode);
}

namespace winapi
{
    DWORD IsAppRunningAsAdminMode(PBOOL IsAdmin);
    std::string GetLastErrorStdStr(DWORD errorCode = 0);
}
