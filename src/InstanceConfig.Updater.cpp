#include "Updater.h"
#include "InstanceConfig.hpp"

bool models::InstanceConfig::ExtractSelfUpdater() const
{
	const HRSRC updater_res = FindResource(appInstance, MAKEINTRESOURCE(IDR_DLL_SELF_UPDATER), RT_RCDATA);
	const int updater_size = static_cast<int>(SizeofResource(appInstance, updater_res));
	const LPVOID updater_data = LockResource(LoadResource(appInstance, updater_res));

	std::stringstream ss;
	ss << appPath.string() << ":updater.dll";
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
		!ret || bytesWritten < updater_size)
	{
		spdlog::error("Failed to write resource to file, error: {}", GetLastError());
		CloseHandle(self);
		return false;
	}

	CloseHandle(self);

	return true;
}
