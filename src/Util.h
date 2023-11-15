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
    void toCamelCase(std::string & s);
    void toSemVerCompatible(std::string & s);
    void stripNulls(std::string & s);
    void stripNulls(std::wstring & s);
}

namespace winapi
{
    semver::version GetWin32ResourceFileVersion(const std::filesystem::path& filePath);
    semver::version GetWin32ResourceProductVersion(const std::filesystem::path& filePath);
    DWORD IsAppRunningAsAdminMode(PBOOL IsAdmin);
    std::string GetLastErrorStdStr(DWORD errorCode = 0);
    BOOL VerifyEmbeddedSignature(LPCWSTR pwszSourceFile);
}

