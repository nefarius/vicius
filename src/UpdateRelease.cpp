#include "pch.h"
#include "Util.h"
#include "UpdateRelease.hpp"


semver::version models::UpdateRelease::GetSemVersion() const
{
    try
    {
        // trim whitespaces and potential "v" prefix
        std::string value = util::trim(version, "v \t");
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
        // trim whitespaces and potential "v" prefix
        std::string value = util::trim(detectionVersion.value(), "v \t");
        util::toSemVerCompatible(value);

        return semver::version::parse(value);
    }
    catch (const std::exception& e)
    {
        spdlog::debug("Error parsing update version `{}`: {}", version, e.what());
    }
    catch (...)
    {
        spdlog::debug("Unknown exception parsing update detection version `{}`", version);

        return std::nullopt;
    }
}
