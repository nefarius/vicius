#include "Updater.h"
#include "InstanceConfig.hpp"

#include <tchar.h>


bool models::InstanceConfig::ExtractSelfUpdater() const
{
	const HRSRC updater_res = FindResource(appInstance, MAKEINTRESOURCE(IDR_DLL_SELF_UPDATER), RT_RCDATA);
	const int updater_size = static_cast<int>(SizeofResource(appInstance, updater_res));
	const LPVOID updater_data = LockResource(LoadResource(appInstance, updater_res));

	std::stringstream ss;
	ss << appPath.string() << NV_UPDATER_ADS_NAME;
	const auto ads = ss.str();

	const HANDLE self = CreateFileA(
		ads.c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (self == INVALID_HANDLE_VALUE)
	{
		spdlog::error("Failed to open ADS handle, error: {}", GetLastError());
		return false;
	}

	DWORD bytesWritten = 0;

	// write DLL resource to our self as ADS
	if (const auto ret = WriteFile(self, updater_data, updater_size, &bytesWritten, nullptr);
		!ret || bytesWritten < static_cast<DWORD>(updater_size))
	{
		spdlog::error("Failed to write resource to file, error: {}", GetLastError());
		CloseHandle(self);
		return false;
	}

	CloseHandle(self);

	return true;
}

bool models::InstanceConfig::HasWritePermissions() const
{
	return _access(appPath.string().c_str(), 06) == 0;
}

bool models::InstanceConfig::RunSelfUpdater() const
{
	const auto workDir = appPath.parent_path();
	std::stringstream dllPath, procArgs;
	dllPath << appPath.string() << NV_UPDATER_ADS_NAME;
	const auto ads = dllPath.str();
	spdlog::debug("ads = {}", ads);

	const auto runDll = "rundll32.exe";

	// build CLI args
	procArgs
		<< "\"" << ads << "\",PerformUpdate"
		<< " --silent"
		<< " --pid " << GetCurrentProcessId()
		<< " --path \"" << appPath.string() << "\""
		<< " --url \"" << remote.instance.latestUrl << "\"";

	const auto args = procArgs.str();
	spdlog::debug("args = {}", args);

	// if we can write to our directory, spawn under current user
	if (HasWritePermissions())
	{
		spdlog::debug("Running with regular privileges");

		STARTUPINFOA si = {sizeof(STARTUPINFOA)};
		PROCESS_INFORMATION pi{};

		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		if (!CreateProcessA(
			runDll,
			const_cast<LPSTR>(args.c_str()),
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			workDir.string().c_str(),
			&si,
			&pi
		))
		{
			spdlog::error("Failed to run updater process, error: {}", GetLastError());
			return false;
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	// request elevation
	else
	{
		spdlog::debug("Requesting running with elevated privileges");

		SHELLEXECUTEINFOA shExInfo = {0};
		shExInfo.cbSize = sizeof(shExInfo);
		shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		shExInfo.hwnd = nullptr;
		shExInfo.lpVerb = "runas";
		shExInfo.lpFile = runDll;
		shExInfo.lpParameters = args.c_str();
		shExInfo.lpDirectory = workDir.string().c_str();
		shExInfo.nShow = SW_HIDE;
		shExInfo.hInstApp = nullptr;

		if (!ShellExecuteExA(&shExInfo))
		{
			spdlog::error("Failed to run elevated updater process, error: {}", GetLastError());
			return false;
		}

		CloseHandle(shExInfo.hProcess);
	}

	return true;
}