#pragma once

#include "UpdateRelease.hpp"
#include "../Util.h"

namespace models
{
    class ExitCodeCheck;

    /**
     * \brief Update instance configuration. Parameters applying to the entire product/tenant.
     */
    class UpdateConfig
    {
    public:
        /** True to disable, false to enable the updates globally */
        std::optional<bool> updatesDisabled;
        /** The latest updater version available */
        std::optional<std::string> latestVersion;
        /** URL of the latest updater binary */
        std::optional<std::string> latestUrl;
        /**
         * \brief Optional checksum of the self-updater binary at latestUrl.
         * Passed to the self-updater DLL so it can verify the download before swapping.
         */
        std::optional<ChecksumParameters> latestChecksum;
        /** Optional URL pointing to an emergency announcement web page */
        std::optional<std::string> emergencyUrl;
        /** The exit code parameters */
        std::optional<ExitCodeCheck> exitCode;
        /** URL of a help article */
        std::optional<std::string> helpUrl;
        /** URL of the error article */
        std::optional<std::string> errorFallbackUrl;
        /** If true, the downloaded setup is launched elevated (As Administrator) via the "runas" verb */
        std::optional<bool> runAsAdmin;

        /**
         * \brief Converts the version string to a SemVer type.
         * \return The parsed version.
         */
        semver::version GetLatestUpdaterSemVersion() const
        {
            if (!latestVersion.has_value())
            {
                return semver::version{0, 0, 0};
            }

            try
            {
                // Normalise the same way release versions are handled so common
                // Win32 forms parse cleanly: trim whitespace, strip a leading "v",
                // and map 4-segment versions (e.g. "999.0.0.0") onto semver via
                // toSemVerCompatible ("1.2.3.4" -> "1.2.3+4").
                std::string value = util::trim(latestVersion.value());
                if (!value.empty() && value.front() == 'v')
                {
                    value.erase(value.begin());
                }
                util::toSemVerCompatible(value);
                return semver::version::parse(value);
            }
            catch (...)  // couldn't convert version string
            {
                return semver::version{0, 0, 0};
            }
        }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
      UpdateConfig, updatesDisabled, latestVersion, latestUrl, latestChecksum, emergencyUrl, exitCode, helpUrl, errorFallbackUrl, runAsAdmin)
}
