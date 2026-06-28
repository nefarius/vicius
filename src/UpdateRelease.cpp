#include "pch.h"
#include "Util.h"
#include "UpdateRelease.hpp"


semver::version models::UpdateRelease::GetSemVersion() const
{
    try
    {
        // trim whitespace, then strip a leading "v" prefix only
        std::string value = util::trim(version);
        if (!value.empty() && value.front() == 'v')
        {
            value.erase(value.begin());
        }
        util::toSemVerCompatible(value);

        return semver::version::parse(value);
    }
    catch (const std::exception& e)
    {
        spdlog::debug("Error parsing update version `{}`: {}", version, e.what());
    }
    catch (...)
    {
        spdlog::debug("Unknown exception parsing update version `{}`", version);
    }
    return semver::version{0, 0, 0};
}

std::optional<semver::version> models::UpdateRelease::GetDetectionSemVersion() const
{
    if (!detectionVersion.has_value())
    {
        // fallback value is mandatory
        return GetSemVersion();
    }

    try
    {
        // trim whitespace, then strip a leading "v" prefix only
        std::string value = util::trim(detectionVersion.value());
        if (!value.empty() && value.front() == 'v')
        {
            value.erase(value.begin());
        }
        util::toSemVerCompatible(value);

        return semver::version::parse(value);
    }
    catch (const std::exception& e)
    {
        spdlog::debug("Error parsing update detection version `{}`: {}", detectionVersion.value(), e.what());
    }
    catch (...)
    {
        spdlog::debug("Unknown exception parsing update detection version `{}`", detectionVersion.value());
    }

    // Fall back to the primary release version, consistent with the no-detectionVersion path.
    return GetSemVersion();
}
