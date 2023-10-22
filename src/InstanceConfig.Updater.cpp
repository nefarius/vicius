#include "pch.h"
#include "Common.h"
#include "InstanceConfig.hpp"


std::tuple<bool, std::string> models::InstanceConfig::ExtractSelfUpdater() const
{
	const HRSRC updater_res = FindResource(appInstance, MAKEINTRESOURCE(IDR_DLL_SELF_UPDATER), RT_RCDATA);
	const int updater_size = static_cast<int>(SizeofResource(appInstance, updater_res));
	const LPVOID updater_data = LockResource(LoadResource(appInstance, updater_res));

	std::stringstream ss;
	ss << appPath.string() << NV_ADS_UPDATER_NAME;
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
		return std::make_tuple(false, winapi::GetLastErrorStdStr());
	}

	DWORD bytesWritten = 0;

	// write DLL resource to our self as ADS
	if (const auto ret = WriteFile(self, updater_data, updater_size, &bytesWritten, nullptr);
		!ret || bytesWritten < static_cast<DWORD>(updater_size))
	{
		const DWORD error = GetLastError();
		CloseHandle(self);
		SetLastError(error);
		return std::make_tuple(false, winapi::GetLastErrorStdStr());
	}

	CloseHandle(self);

	return std::make_tuple(true, "OK");
}

bool models::InstanceConfig::HasWritePermissions() const
{
	const HANDLE self = CreateFileA(
		appPath.string().c_str(),
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	const DWORD error = GetLastError();

	if (self != INVALID_HANDLE_VALUE) CloseHandle(self);

	// error is ERROR_SHARING_VIOLATION when we can write
	// and ERROR_ACCESS_DENIED if missing permissions

	return error == ERROR_SHARING_VIOLATION;
}

bool models::InstanceConfig::RunSelfUpdater() const
{
	const auto workDir = appPath.parent_path();
	std::stringstream dllPath;
	dllPath << appPath.string() << NV_ADS_UPDATER_NAME;
	const auto ads = dllPath.str();
	spdlog::debug("ads = {}", ads);

	const auto runDll = "rundll32.exe";

	// if we can write to our directory, spawn under current user
	if (HasWritePermissions())
	{
		spdlog::debug("Running with regular privileges");

		STARTUPINFOA si = {sizeof(STARTUPINFOA)};
		PROCESS_INFORMATION pi{};

		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		std::stringstream argsStream;
		// build CLI args
		argsStream
			<< "rundll32 \"" << ads << "\",PerformUpdate"
			<< " --silent"
			<< " --log-level " << magic_enum::enum_name(spdlog::get_level())
			<< " --pid " << GetCurrentProcessId()
			<< " --path \"" << appPath.string() << "\""
			<< " --url \"" << remote.instance.latestUrl.value() << "\"";
		const auto args = argsStream.str();
		spdlog::debug("args = {}", args);

		if (!CreateProcessA(
			nullptr,
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

		spdlog::debug("Process launched");

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	// request elevation
	else
	{
		spdlog::debug("Requesting running with elevated privileges");

		std::stringstream argsStream;
		// build CLI args
		argsStream
			<< "\"" << ads << "\",PerformUpdate"
			<< " --silent"
			<< " --log-level " << magic_enum::enum_name(spdlog::get_level())
			<< " --pid " << GetCurrentProcessId()
			<< " --path \"" << appPath.string() << "\""
			<< " --url \"" << remote.instance.latestUrl.value() << "\"";
		const auto args = argsStream.str();
		spdlog::debug("args = {}", args);

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

		spdlog::debug("Process launched");

		CloseHandle(shExInfo.hProcess);
	}

	return true;
}
