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

    /**
     * \brief Three-way compares two versions, honouring a 4th ("revision") segment.
     *
     * SemVer only models major.minor.patch(+prerelease); toSemVerCompatible() stashes
     * the optional 4th segment of common Win32 versions (e.g. "1.2.3.4") as numeric
     * build metadata ("1.2.3+4"). Standard SemVer precedence ignores build metadata,
     * which would silently drop that 4th segment from comparisons. This helper applies
     * normal SemVer precedence first and, when the core/prerelease parts are equal,
     * breaks the tie on the numeric build metadata (a missing/non-numeric value counts
     * as 0, so "1.2.3" and "1.2.3.0" compare equal while "1.2.3.4" outranks both).
     *
     * \return Negative if a precedes b, 0 if equal, positive if a succeeds b.
     */
    int CompareVersions(const semver::version& a, const semver::version& b);
    void stripNulls(std::string& s);
    void stripNulls(std::wstring& s);
}

namespace winapi
{
    std::expected<semver::version, std::string> GetWin32ResourceFileVersion(const std::filesystem::path& filePath);
    std::expected<semver::version, std::string> GetWin32ResourceProductVersion(const std::filesystem::path& filePath);
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
     * \brief Reads HKCU\...\Themes\Personalize!AppsUseLightTheme to determine whether
     *        the user has selected a light application theme in Windows Settings.
     * \return true if light theme is active, false for dark; or an unexpected string
     *         describing the registry read failure so callers can log and fall back.
     */
    [[nodiscard]] std::expected<bool, std::string> IsLightThemeActive();

    /**
     * \brief Reads the system accent color via DwmGetColorizationColor and converts it to
     *        a linear ImVec4 suitable for ImGui style colors. Falls back to the Win11
     *        Fluent default blue (#60CDFF dark / #0078D4 light) if DWM cannot supply a value.
     *        The result is cached until InvalidateAccentColorCache() is called.
     * \return Accent color as ImVec4 (R, G, B, A) with full opacity.
     */
    ImVec4 GetAccentColor();

    /**
     * \brief Returns a lighter/desaturated variant of the accent color for hover states.
     *        Derived from the cached accent color; no extra I/O.
     */
    ImVec4 GetAccentColorHovered();

    /**
     * \brief Returns a darker/more-opaque variant of the accent color for pressed states.
     *        Derived from the cached accent color; no extra I/O.
     */
    ImVec4 GetAccentColorActive();

    /**
     * \brief Invalidates the cached accent color so the next call to GetAccentColor (and its
     *        variants) will re-read the registry and DWM. Call this from WndProc on
     *        WM_SETTINGCHANGE (ImmersiveColorSet) and WM_DWMCOLORIZATIONCOLORCHANGED.
     */
    void InvalidateAccentColorCache();

    /**
     * \brief Decodes a standard Base64 string into raw bytes using CryptStringToBinaryA.
     * \param b64 The Base64-encoded input string.
     * \return Decoded bytes on success; unexpected error string on failure.
     * \remarks Rejects inputs larger than 1 MiB as a safety guard against server-supplied data.
     */
    [[nodiscard]] std::expected<std::vector<uint8_t>, std::string> DecodeBase64(const std::string& b64);

    /**
     * \brief Creates an HICON from an in-memory Windows .ico buffer.
     *
     * Validates the ICONDIR/ICONDIRENTRY structure, picks the best-matching entry for
     * the requested dimensions and delegates to CreateIconFromResourceEx so both
     * legacy BMP-compressed and Vista+ PNG-compressed icon entries are supported.
     *
     * \param ico  Raw bytes of a valid .ico file.
     * \param cx   Desired icon width  (e.g. GetSystemMetrics(SM_CXICON)).
     * \param cy   Desired icon height (e.g. GetSystemMetrics(SM_CYICON)).
     * \return A valid HICON on success; unexpected error string on failure.
     * \remarks The caller is responsible for calling DestroyIcon on the returned handle.
     */
    [[nodiscard]] std::expected<HICON, std::string> CreateIconFromIcoBuffer(const std::vector<uint8_t>& ico, int cx, int cy);
}
