#include "pch.h"
#include <InstanceConfig.hpp>

#define NV_LEGACY_POSTPONE_KEY_TEMPLATE      "SOFTWARE\\Nefarius Software Solutions e.U.\\{}\\Postpone"
#define NV_LEGACY_USER_OPTIONS_KEY_TEMPLATE  "SOFTWARE\\Nefarius Software Solutions e.U.\\Vicius\\{}"
#define NV_ISOLATED_STATE_ROOT_TEMPLATE      "SOFTWARE\\Nefarius Software Solutions e.U.\\Vicius\\State\\v1\\{}"
#define NV_POSTPONE_TS_VALUE_NAME            L"LastTimestamp"
#define NV_UPDATES_DISABLED_VALUE_NAME       L"UpdatesDisabled"

namespace
{
    std::string RegError(const winreg::RegResult& result)
    {
        return ConvertWideToANSI(result.ErrorMessage());
    }

    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }

        const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                               nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
        {
            return ConvertWideToANSI(wide);
        }

        std::string utf8(static_cast<size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                            utf8.data(), needed, nullptr, nullptr);
        return utf8;
    }

    std::wstring NormalizeUpdaterPath(const std::filesystem::path& path)
    {
        wchar_t full[MAX_PATH + 1] = {};
        const DWORD fullLen = GetFullPathNameW(path.c_str(), MAX_PATH + 1, full, nullptr);
        std::wstring normalized = (fullLen > 0 && fullLen <= MAX_PATH)
                                      ? std::wstring(full, fullLen)
                                      : path.wstring();

        wchar_t longPath[MAX_PATH + 1] = {};
        const DWORD longLen = GetLongPathNameW(normalized.c_str(), longPath, MAX_PATH + 1);
        if (longLen > 0 && longLen <= MAX_PATH)
        {
            normalized.assign(longPath, longLen);
        }

        CharLowerBuffW(normalized.data(), static_cast<DWORD>(normalized.size()));
        return normalized;
    }

    bool IsMissingKey(const winreg::RegResult& result)
    {
        return result.Code() == ERROR_FILE_NOT_FOUND || result.Code() == ERROR_PATH_NOT_FOUND;
    }

    std::expected<void, std::string> DeleteValueIfPresent(winreg::RegKey& key, const wchar_t* valueName)
    {
        if (const winreg::RegResult result = key.TryDeleteValue(valueName); !result)
        {
            if (result.Code() == ERROR_FILE_NOT_FOUND)
            {
                return {};
            }
            return std::unexpected(std::format("Failed to delete {}: {}",
                                               ConvertWideToANSI(valueName), RegError(result)));
        }
        return {};
    }

    std::expected<void, std::string> PurgeValueAt(const std::wstring& subKey, const wchar_t* valueName)
    {
        winreg::RegKey key;
        if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey, KEY_ALL_ACCESS); !result)
        {
            if (IsMissingKey(result))
            {
                return {};
            }
            return std::unexpected(std::format("Failed to open {}: {}", ConvertWideToANSI(subKey), RegError(result)));
        }

        return DeleteValueIfPresent(key, valueName);
    }

    std::optional<SYSTEMTIME> ReadPostponeTimestamp(winreg::RegKey& key)
    {
        const auto ret = key.TryGetBinaryValue(NV_POSTPONE_TS_VALUE_NAME);
        if (!ret.IsValid())
        {
            return std::nullopt;
        }

        const auto& bytes = ret.GetValue();
        if (bytes.size() != sizeof(SYSTEMTIME))
        {
            spdlog::warn("Ignoring malformed postpone timestamp ({} bytes, expected {})",
                         bytes.size(), sizeof(SYSTEMTIME));
            return std::nullopt;
        }

        SYSTEMTIME time = {};
        memcpy_s(&time, sizeof(SYSTEMTIME), bytes.data(), sizeof(SYSTEMTIME));
        return time;
    }

    bool IsTimestampInPostponeWindow(const SYSTEMTIME& last)
    {
        SYSTEMTIME current = {};
        GetSystemTime(&current);

        FILETIME ftLhs = {}, ftRhs = {};
        SystemTimeToFileTime(&current, &ftLhs);
        SystemTimeToFileTime(&last, &ftRhs);

        const std::chrono::file_clock::duration dLhs{
            (static_cast<int64_t>(ftLhs.dwHighDateTime) << 32) | ftLhs.dwLowDateTime};
        const std::chrono::file_clock::duration dRhs{
            (static_cast<int64_t>(ftRhs.dwHighDateTime) << 32) | ftRhs.dwLowDateTime};

        const auto hours = std::chrono::duration_cast<std::chrono::hours>(dLhs - dRhs).count();
        return hours < 24;
    }
}

std::filesystem::path models::InstanceConfig::GetOriginalUpdaterPath() const
{
    if (isTemporaryCopy && parentAppPath.has_value())
    {
        spdlog::debug("State identity uses parent updater path {}", parentAppPath->string());
        return parentAppPath.value();
    }
    return appPath;
}

std::string models::InstanceConfig::GetStateIdentityHash() const
{
    std::string typed;
    if (stateId.has_value() && !stateId->empty())
    {
        typed = std::format("id:{}", *stateId);
    }
    else
    {
        typed = std::format("path:{}", WideToUtf8(NormalizeUpdaterPath(GetOriginalUpdaterPath())));
    }

    SHA256 sha;
    sha.add(typed.data(), typed.size());
    return sha.getHash();
}

namespace
{
    std::wstring IsolatedRoot(const models::InstanceConfig& cfg)
    {
        return ConvertAnsiToWide(std::format(NV_ISOLATED_STATE_ROOT_TEMPLATE, cfg.GetStateIdentityHash()));
    }

    std::wstring IsolatedPostpone(const models::InstanceConfig& cfg)
    {
        return IsolatedRoot(cfg) + L"\\Postpone";
    }

    std::wstring IsolatedUserOptions(const models::InstanceConfig& cfg)
    {
        return IsolatedRoot(cfg);
    }

    std::wstring LegacyPostpone(const std::string& appFilename)
    {
        return ConvertAnsiToWide(std::format(NV_LEGACY_POSTPONE_KEY_TEMPLATE, appFilename));
    }

    std::wstring LegacyUserOptions(const std::string& appFilename)
    {
        return ConvertAnsiToWide(std::format(NV_LEGACY_USER_OPTIONS_KEY_TEMPLATE, appFilename));
    }

    std::expected<void, std::string> EnsureNonVolatileRoot(const std::wstring& rootKey)
    {
        winreg::RegKey key;
        if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER, rootKey, KEY_READ | KEY_WRITE); !result)
        {
            return std::unexpected(std::format("Failed to create {}: {}", ConvertWideToANSI(rootKey), RegError(result)));
        }
        return {};
    }
}

std::expected<void, std::string> models::InstanceConfig::SetPostponeData()
{
    const auto root = IsolatedRoot(*this);
    if (const auto ensured = EnsureNonVolatileRoot(root); !ensured)
    {
        spdlog::error("{}", ensured.error());
        return std::unexpected(ensured.error());
    }

    winreg::RegKey key;
    const auto subKey = IsolatedPostpone(*this);
    if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER,
                                                       subKey,
                                                       KEY_ALL_ACCESS,
                                                       REG_OPTION_VOLATILE,
                                                       nullptr,
                                                       nullptr);
        !result)
    {
        spdlog::error("Failed to create {}", ConvertWideToANSI(subKey));
        return std::unexpected(std::format("Failed to create postpone key: {}", RegError(result)));
    }

    SYSTEMTIME time = {};
    GetSystemTime(&time);

    if (const auto result = key.TrySetBinaryValue(NV_POSTPONE_TS_VALUE_NAME, &time, sizeof(SYSTEMTIME)); !result)
    {
        spdlog::error("Failed to set timestamp value");
        return std::unexpected(std::format("Failed to set postpone timestamp: {}", RegError(result)));
    }

    spdlog::debug("Wrote postpone timestamp under isolated state {}", GetStateIdentityHash());
    return {};
}

std::expected<void, std::string> models::InstanceConfig::PurgePostponeData()
{
    if (const auto isolated = PurgeValueAt(IsolatedPostpone(*this), NV_POSTPONE_TS_VALUE_NAME); !isolated)
    {
        spdlog::error("{}", isolated.error());
        return isolated;
    }

    if (const auto legacy = PurgeValueAt(LegacyPostpone(appFilename), NV_POSTPONE_TS_VALUE_NAME); !legacy)
    {
        spdlog::error("{}", legacy.error());
        return legacy;
    }

    spdlog::debug("Purged postpone data from isolated and legacy locations");
    return {};
}

bool models::InstanceConfig::IsInPostponePeriod()
{
    if (ignorePostponePeriod)
    {
        spdlog::info("User specified to ignore postpone period, skipping check");
        return false;
    }

    auto tryKey = [](const std::wstring& subKey) -> std::optional<SYSTEMTIME>
    {
        winreg::RegKey key;
        if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
        {
            return std::nullopt;
        }
        return ReadPostponeTimestamp(key);
    };

    if (const auto isolated = tryKey(IsolatedPostpone(*this)))
    {
        return IsTimestampInPostponeWindow(*isolated);
    }

    const auto legacy = tryKey(LegacyPostpone(appFilename));
    if (!legacy)
    {
        return false;
    }

    if (!IsTimestampInPostponeWindow(*legacy))
    {
        return false;
    }

    // Copy the still-valid legacy timestamp so this updater (and only this identity)
    // keeps using isolated state after the first read. Leave the legacy value in place
    // so another same-named updater can still migrate it before reboot.
    const auto root = IsolatedRoot(*this);
    if (const auto ensured = EnsureNonVolatileRoot(root); !ensured)
    {
        spdlog::warn("Failed to migrate legacy postpone timestamp: {}", ensured.error());
        return true;
    }

    winreg::RegKey key;
    const auto isolatedKey = IsolatedPostpone(*this);
    if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER,
                                                       isolatedKey,
                                                       KEY_ALL_ACCESS,
                                                       REG_OPTION_VOLATILE,
                                                       nullptr,
                                                       nullptr);
        !result)
    {
        spdlog::warn("Failed to create isolated postpone key during migration: {}", RegError(result));
        return true;
    }

    if (const auto written = key.TrySetBinaryValue(NV_POSTPONE_TS_VALUE_NAME, &*legacy, sizeof(SYSTEMTIME)); !written)
    {
        spdlog::warn("Failed to copy legacy postpone timestamp: {}", RegError(written));
    }
    else
    {
        spdlog::info("Migrated postpone timestamp from legacy filename key to isolated state");
    }

    return true;
}

std::expected<bool, std::string> models::InstanceConfig::AreUpdatesDisabled() const
{
    auto readDisabled = [](const std::wstring& subKey)
        -> std::expected<std::optional<bool>, std::string>
    {
        winreg::RegKey key;
        if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
        {
            if (IsMissingKey(result))
            {
                return std::optional<bool>{};
            }
            return std::unexpected(std::format("Failed to open user-state key: {}", RegError(result)));
        }

        const auto ret = key.TryGetDwordValue(NV_UPDATES_DISABLED_VALUE_NAME);
        if (!ret.IsValid())
        {
            const auto error = ret.GetError();
            if (error.Code() == ERROR_FILE_NOT_FOUND)
            {
                return std::optional<bool>{};
            }
            return std::unexpected(std::format("Failed to read UpdatesDisabled: {}", RegError(error)));
        }

        return std::optional<bool>{ret.GetValue() != 0};
    };

    const auto isolated = readDisabled(IsolatedUserOptions(*this));
    if (!isolated)
    {
        spdlog::error("{}", isolated.error());
        return std::unexpected(isolated.error());
    }
    if (isolated->has_value())
    {
        return isolated->value();
    }

    const auto legacy = readDisabled(LegacyUserOptions(appFilename));
    if (!legacy)
    {
        spdlog::error("{}", legacy.error());
        return std::unexpected(legacy.error());
    }
    if (!legacy->has_value())
    {
        return false;
    }

    const bool disabled = legacy->value();
    if (disabled)
    {
        // Persist onto the isolated key so later writes (and this identity) diverge
        // from other same-named updaters. Leave the legacy value for other migrators.
        winreg::RegKey key;
        const auto subKey = IsolatedUserOptions(*this);
        if (const winreg::RegResult result = key.TryCreate(HKEY_CURRENT_USER, subKey, KEY_READ | KEY_WRITE); !result)
        {
            spdlog::warn("Failed to migrate UpdatesDisabled to isolated key: {}", RegError(result));
        }
        else if (const winreg::RegResult written = key.TrySetDwordValue(NV_UPDATES_DISABLED_VALUE_NAME, 1); !written)
        {
            spdlog::warn("Failed to migrate UpdatesDisabled value: {}", RegError(written));
        }
        else
        {
            spdlog::info("Migrated UpdatesDisabled from legacy filename key to isolated state");
        }
    }

    return disabled;
}

std::expected<void, std::string> models::InstanceConfig::SetUpdatesDisabled(const bool disabled)
{
    winreg::RegKey key;
    const auto subKey = IsolatedUserOptions(*this);

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

    if (const auto isolated = DeleteValueIfPresent(key, NV_UPDATES_DISABLED_VALUE_NAME); !isolated)
    {
        spdlog::error("{}", isolated.error());
        return isolated;
    }

    if (const auto legacy = PurgeValueAt(LegacyUserOptions(appFilename), NV_UPDATES_DISABLED_VALUE_NAME); !legacy)
    {
        spdlog::error("{}", legacy.error());
        return legacy;
    }

    spdlog::info("Update notifications re-enabled by user preference");
    return {};
}
