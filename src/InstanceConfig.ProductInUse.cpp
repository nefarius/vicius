#include "pch.h"
#include <InstanceConfig.hpp>
#include "Common.h"

#include <tlhelp32.h>


bool models::InstanceConfig::HasProductBusyDetection() const
{
    return merged.productBusyDetection.has_value();
}

int models::InstanceConfig::GetProductBusyPollSeconds() const
{
    if (!merged.productBusyDetection.has_value())
    {
        return 60;
    }

    const auto configured = merged.productBusyDetection.value().pollIntervalSeconds.value_or(60);
    return (std::max)(configured, 5); // never spin faster than 5s
}

int models::InstanceConfig::GetProductBusyMaxWaitMinutes() const
{
    if (!merged.productBusyDetection.has_value())
    {
        return NV_PRODUCT_IN_USE_MAX_WAIT_MINUTES;
    }

    const auto configured = merged.productBusyDetection.value().maxWaitMinutes.value_or(NV_PRODUCT_IN_USE_MAX_WAIT_MINUTES);
    return (std::max)(0, (std::min)(configured, NV_PRODUCT_IN_USE_MAX_WAIT_MINUTES));
}

std::expected<bool, std::string> models::InstanceConfig::IsProductInUse() const
{
    if (!merged.productBusyDetection.has_value())
    {
        return false;
    }

    const auto& cfg = merged.productBusyDetection.value();

    // Build lowercase set of image names for O(1) lookup
    std::vector<std::string> lowerImageNames;
    lowerImageNames.reserve(cfg.imageNames.size());
    for (const auto& name : cfg.imageNames)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        lowerImageNames.push_back(std::move(lower));
    }

    // Render executable path templates (if any) up front
    std::vector<std::string> renderedPaths;
    if (cfg.executablePaths.has_value())
    {
        for (const auto& tpl : cfg.executablePaths.value())
        {
            try
            {
                renderedPaths.push_back(RenderInjaTemplate(tpl, {}));
            }
            catch (const std::exception& e)
            {
                spdlog::warn("IsProductInUse: failed to render executable path template '{}': {}", tpl, e.what());
                renderedPaths.push_back(tpl); // fall back to literal path
            }
        }
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return std::unexpected(std::format("CreateToolhelp32Snapshot failed: {:#010x}",
                                           static_cast<uint32_t>(GetLastError())));
    }
    auto snapshotGuard = sg::make_scope_guard([&] { CloseHandle(snapshot); });

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(snapshot, &entry))
    {
        if (GetLastError() == ERROR_NO_MORE_FILES)
        {
            return false; // no processes at all (should not happen in practice)
        }
        return std::unexpected(std::format("Process32FirstW failed: {:#010x}",
                                           static_cast<uint32_t>(GetLastError())));
    }

    do
    {
        // Convert wide image name to narrow for comparison
        const std::string narrowName = ConvertWideToANSI(entry.szExeFile);
        std::string lowerName = narrowName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Selector 1: image base name (cheap, case-insensitive)
        if (!lowerImageNames.empty())
        {
            const bool nameMatches = std::find(lowerImageNames.begin(), lowerImageNames.end(), lowerName)
                                     != lowerImageNames.end();
            if (nameMatches)
            {
                spdlog::debug("IsProductInUse: matched process by name '{}' (PID {})", narrowName, entry.th32ProcessID);
                return true;
            }
        }

        // Selector 2: full executable path (independent of selector 1 — evaluated for every process
        // whenever executablePaths is configured, regardless of whether imageNames matched)
        if (!renderedPaths.empty())
        {
            const auto fullPath = nefarius::winapi::GetProcessFullPath<std::string>(entry.th32ProcessID);
            if (fullPath)
            {
                const std::string& pathStr = std::get<std::string>(fullPath.value());
                for (const auto& renderedPath : renderedPaths)
                {
                    if (_stricmp(pathStr.c_str(), renderedPath.c_str()) == 0)
                    {
                        spdlog::debug("IsProductInUse: matched process by path '{}' (PID {})",
                                      pathStr, entry.th32ProcessID);
                        return true;
                    }
                }
            }
        }
    }
    while (Process32NextW(snapshot, &entry));

    return false;
}
