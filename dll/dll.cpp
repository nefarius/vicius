// dll.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "dll.h"

static std::string ConvertWideToANSI(const std::wstring& wstr)
{
	int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), static_cast<int>(wstr.length()), nullptr, 0, nullptr,
	                                nullptr);
	std::string str(count, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
	return str;
}

static std::wstring ConvertAnsiToWide(const std::string& str)
{
	int count = MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), &wstr[0], count);
	return wstr;
}

EXTERN_C DLL_API void CALLBACK PerformUpdate(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	int nArgs;

	const LPWSTR* szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	// even with no arguments passed, this is expected to succeed
	if (nullptr == szArglist)
	{
		return;
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

	argh::parser cmdl;

	cmdl.add_params({
		"--pid", // PID of the parent process
		"--url", // latest updater download URL
		"--path", // the target file path
	});

	// we now have the same format as a classic main argv to parse
	cmdl.parse(nArgs, argv.data());

	std::string url;
	if (!(cmdl({"--url"}) >> url))
		return;

	int pid;
	if (!(cmdl({"--pid"}) >> pid))
		return;

	std::string path;
	if (!(cmdl({"--path"}) >> path))
		return;

	std::filesystem::path original = path;
	std::filesystem::path temp = original.parent_path() / "newupdater.exe";
	curlpp::Cleanup myCleanup;

	try
	{
		MoveFileA(original.string().c_str(), temp.string().c_str());

		// download directly to main file stream
		std::ofstream outStream(original, std::ios::binary | std::ofstream::ate);
		outStream << curlpp::options::Url(url);

		DeleteFileA(temp.string().c_str());
	}
	catch (curlpp::RuntimeError& e)
	{
		MessageBoxA(hwnd, e.what(), "Runtime error", MB_OK);
	}
	catch (curlpp::LogicError& e)
	{
		MessageBoxA(hwnd, e.what(), "Logic error", MB_OK);
	}
}
