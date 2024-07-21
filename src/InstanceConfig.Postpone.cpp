#include "pch.h"
#include <InstanceConfig.hpp>

#define NV_POSTPONE_TS_KEY_TEMPLATE "SOFTWARE\\Nefarius Software Solutions e.U.\\{}\\Postpone"
#define NV_POSTPONE_TS_VALUE_NAME   L"LastTimestamp"


void models::InstanceConfig::SetPostponeData()
{
    winreg::RegKey key;
    const auto subKeyTemplate = std::format(NV_POSTPONE_TS_KEY_TEMPLATE, appFilename);
    const auto subKey = ConvertAnsiToWide(subKeyTemplate);

    if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER,
                                                       subKey,
                                                       KEY_ALL_ACCESS,
                                                       REG_OPTION_VOLATILE,  // gets discarded on reboot
                                                       nullptr,
                                                       nullptr);
        !result)
    {
        spdlog::error("Failed to create {}", ConvertWideToANSI(subKey));
        return;
    }

    SYSTEMTIME time = {};
    GetSystemTime(&time);

    if (const auto result = key.TrySetBinaryValue(NV_POSTPONE_TS_VALUE_NAME, &time, sizeof(SYSTEMTIME)); !result)
    {
        spdlog::error("Failed to set timestamp value");
    }
}

bool models::InstanceConfig::PurgePostponeData()
{
    winreg::RegKey key;
    const auto subKeyTemplate = std::format(NV_POSTPONE_TS_KEY_TEMPLATE, appFilename);
    const auto subKey = ConvertAnsiToWide(subKeyTemplate);

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey, KEY_ALL_ACCESS); !result)
    {
        spdlog::error("Failed to open postpone key, error {}", ConvertWideToANSI(result.ErrorMessage()));
        return false;
    }

    if (const winreg::RegResult result = key.TryDeleteValue(NV_POSTPONE_TS_VALUE_NAME); !result)
    {
        spdlog::error("Failed to delete postpone value, error {}", ConvertWideToANSI(result.ErrorMessage()));
        return false;
    }

    return true;
}

bool models::InstanceConfig::IsInPostponePeriod()
{
    if (ignorePostponePeriod)
    {
        spdlog::info("User specified to ignore postpone period, skipping check");
        return false;
    }

    winreg::RegKey key;
    const auto subKeyTemplate = std::format(NV_POSTPONE_TS_KEY_TEMPLATE, appFilename);
    const auto subKey = ConvertAnsiToWide(subKeyTemplate);

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        return false;
    }

    const auto ret = key.TryGetBinaryValue(NV_POSTPONE_TS_VALUE_NAME);

    if (!ret.IsValid())
    {
        return false;
    }

    SYSTEMTIME current = {}, last = {};
    GetSystemTime(&current);
    memcpy_s(&last, sizeof(SYSTEMTIME), ret.GetValue().data(), sizeof(SYSTEMTIME));

    FILETIME ftLhs = {}, ftRhs = {};
    SystemTimeToFileTime(&current, &ftLhs);
    SystemTimeToFileTime(&last, &ftRhs);

    const std::chrono::file_clock::duration dLhs{(static_cast<int64_t>(ftLhs.dwHighDateTime) << 32) | ftLhs.dwLowDateTime};
    const std::chrono::file_clock::duration dRhs{(static_cast<int64_t>(ftRhs.dwHighDateTime) << 32) | ftRhs.dwLowDateTime};

    const auto diffHours = std::chrono::duration_cast<std::chrono::hours>(dLhs - dRhs);
    const auto hours = diffHours.count();

    return hours < 24;
}
