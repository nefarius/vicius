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
	// TEMP directory
	std::string tempPath(MAX_PATH, '\0');
	GetTempPathA(MAX_PATH, tempPath.data());
	// temporary file
	std::string tempFile(MAX_PATH, '\0');
	GetTempFileNameA(tempPath.c_str(), "VICIUS", 0, tempFile.data());
	curlpp::Cleanup myCleanup;
	HANDLE hProcess = nullptr;
	int retries = 10;

	// wait until parent is no more
	do
	{
		if (hProcess) CloseHandle(hProcess);

		// we failed and bail since the rest of the logic will not work
		if (--retries < 1) return;

		hProcess = OpenProcess(
			PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
			FALSE,
			pid
		);

		if (hProcess) Sleep(25);
	}
	while (hProcess);

	try
	{
		// we can not yet directly write to it but move it to free the original name!
		MoveFileA(original.string().c_str(), tempFile.c_str());

		// download directly to main file stream
		std::ofstream outStream(original, std::ios::binary | std::ofstream::ate);
		outStream << curlpp::options::Url(url);

		// delete the file where this ADS sits in (yes, it works!)
		DeleteFileA(tempFile.c_str());
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
