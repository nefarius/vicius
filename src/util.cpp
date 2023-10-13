#include "Updater.h"
#include <algorithm>
#include <argh.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;


namespace util
{
	std::filesystem::path GetImageBasePathW()
	{
		wchar_t myPath[MAX_PATH + 1] = { 0 };

		GetModuleFileNameW(
			reinterpret_cast<HINSTANCE>(&__ImageBase),
			myPath,
			MAX_PATH + 1
		);

		return std::wstring(myPath);
	}

	semver::version GetVersionFromFile(const std::filesystem::path& filePath)
	{
		DWORD verHandle = 0;
		UINT size = 0;
		LPBYTE lpBuffer = nullptr;
		const DWORD verSize = GetFileVersionInfoSizeA(filePath.string().c_str(), &verHandle);
		std::stringstream versionString;

		if (verSize != NULL)
		{
			const auto verData = new char[verSize];

			if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
			{
				if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*) & lpBuffer, &size))
				{
					if (size)
					{
						const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							versionString
								<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
								<< static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
								<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS));
						}
					}
				}
			}
			delete[] verData;
		}

		return semver::version{ versionString.str() };
	}

	bool ParseCommandLineArguments(argh::parser& cmdl)
	{
		int nArgs;

		const LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
		// even with no arguments passed, this is expected to succeed
		if (nullptr == szArglist)
		{
			return false;
		}

		std::vector<const char*> argv;
		std::vector<std::string> narrow;

		for (int i = 0; i < nArgs; i++)
		{
			// Windows gives us wide only, convert each to narrow
			narrow.push_back(ConvertWideToANSI(std::wstring(szArglist[i])));
		}

		argv.resize(nArgs);
		std::ranges::transform(narrow, argv.begin(), [](const std::string& arg) { return arg.c_str(); });

		argv.push_back(nullptr);

		// we now have the same format as a classic main argv to parse
		cmdl.parse(nArgs, argv.data());

		return true;
	}

	std::string trim(const std::string& str, const std::string& whitespace)
	{
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos)
			return ""; // no content

		const auto strEnd = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	bool icompare_pred(unsigned char a, unsigned char b)
	{
		return std::tolower(a) == std::tolower(b);
	}

	bool icompare(std::string const& a, std::string const& b)
	{
		if (a.length() == b.length()) {
			return std::equal(b.begin(), b.end(),
				a.begin(), icompare_pred);
		}
		else {
			return false;
		}
	}
}
