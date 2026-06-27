#pragma once

namespace util
{
    std::filesystem::path GetImageBasePathW();
    semver::version GetVersionFromFile(const std::filesystem::path& filePath);
    [[nodiscard]] std::expected<void, std::string> ParseCommandLineArguments(argh::parser& cmdl);
    std::string trim(const std::string& str, const std::string& whitespace = " \t");
    bool icompare_pred(unsigned char a, unsigned char b);
    bool icompare(const std::string& a, const std::string& b);
    void toCamelCase(std::string& s);
    void toSemVerCompatible(std::string& s);
    void stripNulls(std::string& s);
    void stripNulls(std::wstring& s);
}

namespace winapi
{
    semver::version GetWin32ResourceFileVersion(const std::filesystem::path& filePath);
    semver::version GetWin32ResourceProductVersion(const std::filesystem::path& filePath);
    std::string GetLastErrorStdStr(DWORD errorCode = 0);

    /**
     * Query if 'errorCode' is an MSI error code.
     *
     * @author	Benjamin "Nefarius" Hoeglinger-Stelzer
     * @date	04.02.2024
     *
     * @param 	errorCode	The error code.
     *
     * @returns	True if MSI error code, false if not.
     */
    bool IsMsiExecErrorCode(DWORD errorCode);

    BOOL VerifyEmbeddedSignature(LPCWSTR pwszSourceFile);

    /**
     * \brief Attempts to retrieve the current users' temporary directory.
     * \return The temp directory path on success; unexpected error string on failure.
     */
    [[nodiscard]] std::expected<std::string, std::string> GetUserTemporaryDirectory();

    [[nodiscard]] std::expected<std::string, std::string> GetNewTemporaryFile(_In_opt_ const std::string& parent = std::string());

    /**
     * \brief Attempts to retrieve the %ProgramData% directory.
     * \return The ProgramData path on success; unexpected error string on failure.
     */
    [[nodiscard]] std::expected<std::string, std::string> GetProgramDataPath();

    /**
     * \brief Queries for the current monitor DPI value.
     * \param hWnd Window handle.
     * \return DPI value.
     */
    WORD GetWindowDPI(HWND hWnd);

    /**
     * \brief Gets the refresh rate of the monitor the given window is rendered on.
     * \param hWnd Window handle.
     * \param defaultRate The fallback rate to use if lookup failed.
     * \return The detected refresh rate.
     */
    DWORD GetMonitorRefreshRate(HWND hWnd, DWORD defaultRate = 60);

    /**
     * \brief Enabled or disables immerse dark mode on a window.
     * \param hWnd Window handle.
     * \param useDarkMode TRUE to set dark mode, FALSE for light/default.
     */
    void SetDarkMode(HWND hWnd, BOOL useDarkMode = TRUE);

    /**
     * \brief Reads the system accent color via DwmGetColorizationColor and converts it to
     *        a linear ImVec4 suitable for ImGui style colors. Falls back to the Win11
     *        Fluent default blue (#60CDFF) if DWM cannot supply a value.
     * \return Accent color as ImVec4 (R, G, B, A) with full opacity.
     */
    ImVec4 GetAccentColor();

    /**
     * \brief Returns a lighter/desaturated variant of the accent color for hover states.
     */
    ImVec4 GetAccentColorHovered();

    /**
     * \brief Returns a darker/more-opaque variant of the accent color for pressed states.
     */
    ImVec4 GetAccentColorActive();
}
