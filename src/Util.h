#pragma once

namespace util
{
    std::filesystem::path GetImageBasePathW();
    semver::version GetVersionFromFile(const std::filesystem::path& filePath);
    bool ParseCommandLineArguments(argh::parser& cmdl);
    std::string trim(const std::string& str, const std::string& whitespace = " \t");
    bool icompare_pred(unsigned char a, unsigned char b);
    bool icompare(const std::string& a, const std::string& b);
    bool IsAdmin(int& errorCode);
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
     * \param path The path it will get written to.
     * \return True on success, false otherwise.
     */
    _Must_inspect_result_ bool GetUserTemporaryDirectory(_Inout_ std::string& path);

    bool GetNewTemporaryFile(_Inout_ std::string& path, _In_opt_ const std::string& parent = std::string());

    /**
     * \brief Attempts to retrieve the %ProgramData% directory.
     * \param path The path it will get written to.
     * \return True on success, false otherwise.
     */
    bool GetProgramDataPath(std::string& path);

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
}
