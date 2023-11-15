#include "pch.h"
#include "Util.h"
#include "UpdateRelease.hpp"


semver::version models::UpdateRelease::GetSemVersion() const
{
    try
    {
        std::string value = util::trim(version, "v \t");
        util::toSemVerCompatible(value);
        // trim whitespaces and potential "v" prefix
        return semver::version{value};
    }
    catch (...)
    {
        return semver::version{0, 0, 0};
    }
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
        const std::string value = util::trim(detectionVersion.value(), "v \t");

        // trim whitespaces and potential "v" prefix
        return semver::version{value};
    }
    catch (...)
    {
        return std::nullopt;
    }
}
