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

    BOOL SafeGetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo);
}
