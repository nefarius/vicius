#include "pch.h"
#include <InstanceConfig.hpp>

#define NV_POSTPONE_TS_KEY_TEMPLATE "SOFTWARE\\Nefarius Software Solutions e.U.\\{}\\Postpone"
#define NV_POSTPONE_TS_VALUE_NAME   L"LastTimestamp"

// Persistent (non-volatile) sibling of the volatile Postpone key. Creating the
// preference under the Postpone parent would inherit REG_OPTION_VOLATILE from
// existing same-session postpone writes and disappear at reboot.
#define NV_USER_STATE_KEY_TEMPLATE       "SOFTWARE\\Nefarius Software Solutions e.U.\\Vicius\\{}"
#define NV_UPDATES_DISABLED_VALUE_NAME   L"UpdatesDisabled"

namespace
{
    std::wstring UserStateSubKey(const std::string& appFilename)
    {
        return ConvertAnsiToWide(std::format(NV_USER_STATE_KEY_TEMPLATE, appFilename));
    }

    std::string RegError(const winreg::RegResult& result)
    {
        return ConvertWideToANSI(result.ErrorMessage());
    }
}


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

std::expected<void, std::string> models::InstanceConfig::PurgePostponeData()
{
    winreg::RegKey key;
    const auto subKeyTemplate = std::format(NV_POSTPONE_TS_KEY_TEMPLATE, appFilename);
    const auto subKey = ConvertAnsiToWide(subKeyTemplate);

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey, KEY_ALL_ACCESS); !result)
    {
        if (result.Code() == ERROR_FILE_NOT_FOUND || result.Code() == ERROR_PATH_NOT_FOUND)
        {
            spdlog::debug("Postpone key not present, nothing to purge");
            return {};
        }
        spdlog::error("Failed to open postpone key, error {}", ConvertWideToANSI(result.ErrorMessage()));
        return std::unexpected(std::format("Failed to open postpone key: {}", ConvertWideToANSI(result.ErrorMessage())));
    }

    if (const winreg::RegResult result = key.TryDeleteValue(NV_POSTPONE_TS_VALUE_NAME); !result)
    {
        if (result.Code() == ERROR_FILE_NOT_FOUND)
        {
            spdlog::debug("Postpone value not present, nothing to purge");
            return {};
        }
        spdlog::error("Failed to delete postpone value, error {}", ConvertWideToANSI(result.ErrorMessage()));
        return std::unexpected(std::format("Failed to delete postpone value: {}", ConvertWideToANSI(result.ErrorMessage())));
    }

    return {};
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

std::expected<bool, std::string> models::InstanceConfig::AreUpdatesDisabled() const
{
    winreg::RegKey key;
    const auto subKey = UserStateSubKey(appFilename);

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        if (result.Code() == ERROR_FILE_NOT_FOUND || result.Code() == ERROR_PATH_NOT_FOUND)
        {
            return false;
        }

        spdlog::error("Failed to open user-state key, error {}", RegError(result));
        return std::unexpected(std::format("Failed to open user-state key: {}", RegError(result)));
    }

    const auto ret = key.TryGetDwordValue(NV_UPDATES_DISABLED_VALUE_NAME);
    if (!ret.IsValid())
    {
        const auto error = ret.GetError();
        if (error.Code() == ERROR_FILE_NOT_FOUND)
        {
            return false;
        }

        spdlog::error("Failed to read UpdatesDisabled, error {}", RegError(error));
        return std::unexpected(std::format("Failed to read UpdatesDisabled: {}", RegError(error)));
    }

    return ret.GetValue() != 0;
}

std::expected<void, std::string> models::InstanceConfig::SetUpdatesDisabled(const bool disabled)
{
    winreg::RegKey key;
    const auto subKey = UserStateSubKey(appFilename);

    if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER, subKey, KEY_READ | KEY_WRITE); !result)
    {
        spdlog::error("Failed to create user-state key, error {}", RegError(result));
        return std::unexpected(std::format("Failed to create user-state key: {}", RegError(result)));
    }

    if (disabled)
    {
        if (const winreg::RegResult result = key.TrySetDwordValue(NV_UPDATES_DISABLED_VALUE_NAME, 1); !result)
        {
            spdlog::error("Failed to set UpdatesDisabled, error {}", RegError(result));
            return std::unexpected(std::format("Failed to set UpdatesDisabled: {}", RegError(result)));
        }

        spdlog::info("Update notifications disabled by user preference");
        return {};
    }

    if (const winreg::RegResult result = key.TryDeleteValue(NV_UPDATES_DISABLED_VALUE_NAME); !result)
    {
        if (result.Code() == ERROR_FILE_NOT_FOUND)
        {
            return {};
        }

        spdlog::error("Failed to clear UpdatesDisabled, error {}", RegError(result));
        return std::unexpected(std::format("Failed to clear UpdatesDisabled: {}", RegError(result)));
    }

    spdlog::info("Update notifications re-enabled by user preference");
    return {};
}
