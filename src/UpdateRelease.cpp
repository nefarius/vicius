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
        // trim whitespaces and potential "v" prefix
        std::string value = util::trim(detectionVersion.value(), "v \t");
        util::toSemVerCompatible(value);
        
        return semver::version{value};
    }
    catch (...)
    {
        return std::nullopt;
    }
}
