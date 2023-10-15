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

static std::string GetRandomString()
{
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());

	std::ranges::shuffle(str, generator);

	return str.substr(0, 8);
}


EXTERN_C DLL_API void CALLBACK PerformUpdate(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hinst);
	UNREFERENCED_PARAMETER(lpszCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

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

	bool silent = cmdl[{"--silent"}];

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
	// hint: we must remain on the same drive, or renaming will fail!
	std::filesystem::path randomName(GetRandomString());
	std::filesystem::path temp = original.parent_path() / randomName;
	std::string tempFile = temp.string();
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
			PROCESS_QUERY_INFORMATION,
			FALSE,
			pid
		);

		if (hProcess) Sleep(25);
	}
	while (hProcess);

	std::ofstream outStream;
	const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
	outStream.exceptions(exceptionMask);

	try
	{
		// we can not yet directly write to it but move it to free the original name!
		MoveFileA(original.string().c_str(), tempFile.c_str());
		SetFileAttributesA(tempFile.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

		// download directly to main file stream
		outStream.open(original, std::ios::binary | std::ofstream::ate);
		outStream << curlpp::options::Url(url);

		if (DeleteFileA(tempFile.c_str()) == 0)
		{
			// if it still fails, schedule nuking the old file at next reboot
			MoveFileExA(
				tempFile.c_str(),
				nullptr,
				MOVEFILE_DELAY_UNTIL_REBOOT
			);
		}

		return;
	}
	catch (curlpp::RuntimeError& e)
	{
		if (!silent) MessageBoxA(hwnd, e.what(), "Runtime error", MB_ICONERROR | MB_OK);
	}
	catch (curlpp::LogicError& e)
	{
		if (!silent) MessageBoxA(hwnd, e.what(), "Logic error", MB_ICONERROR | MB_OK);
	}
	catch (std::ios_base::failure& e)
	{
		if (!silent) MessageBoxA(hwnd, e.what(), "I/O error", MB_ICONERROR | MB_OK);
	}
	catch (...)
	{
		// welp
	}

	// restore original file on failure
	CopyFileA(tempFile.c_str(), original.string().c_str(), FALSE);
	SetFileAttributesA(original.string().c_str(), FILE_ATTRIBUTE_NORMAL);
	// attempt to delete temporary copy
	if (DeleteFileA(tempFile.c_str()) == 0)
	{
		// if it still fails, schedule nuking the old file at next reboot
		MoveFileExA(
			tempFile.c_str(),
			nullptr,
			MOVEFILE_DELAY_UNTIL_REBOOT
		);
	}
}
