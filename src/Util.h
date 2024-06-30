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
    DWORD IsAppRunningAsAdminMode(PBOOL IsAdmin);
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
     * \brief Checks whether a given directory exists.
     * \param dirName The path to check.
     * \return True if it exists, false otherwise.
     */
    bool DirectoryExists(const std::string& dirName);

    /**
     * \brief Checks if the given directory exists and creates it otherwise. Nested paths are supported.
     * \param dirName The path.
     * \return True if directory exists or was successfully created, false otherwise.
     */
    bool DirectoryCreate(const std::string& dirName);

    /**
     * \brief Attempts to retrieve the current users' temporary directory.
     * \param path The path it will get written to.
     * \return True on success, false otherwise.
     */
    bool GetUserTemporaryPath(std::string& path);

    /**
     * \brief Attempts to retrieve the %ProgramData% directory.
     * \param path The path it will get written to.
     * \return True on success, false otherwise.
     */
    bool GetProgramDataPath(std::string& path);

    /**
     * \brief Retrieves information about the current system to an application running under WOW64. 
     * If the function is called from a 64-bit application, it is equivalent to the GetSystemInfo 
     * function. If the function is called from an x86 or x64 application running on a 64-bit system 
     * that does not have an Intel64 or x64 processor (such as ARM64), it will return information as 
     * if the system is x86 only if x86 emulation is supported (or x64 if x64 emulation is also 
     * supported).
     * \param lpSystemInfo A pointer to a SYSTEM_INFO structure that receives the information.
     * \return TRUE on success, FALSE otherwise.
     */
    BOOL SafeGetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo);

    /**
     * \brief Queries for the current monitor DPI value.
     * \param hWnd Window handle.
     * \return DPI value.
     */
    WORD GetWindowDPI(HWND hWnd);

    /**
     * \brief Gets the refresh rate of the monitor the given window is rendered on.
     * \param hWnd Windows handle.
     * \param defaultRate The fallback rate to use if lookup failed.
     * \return The detected refresh rate.
     */
    DWORD GetMonitorRefreshRate(HWND hWnd, DWORD defaultRate = 60);
}
