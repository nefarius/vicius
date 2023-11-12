#pragma once

namespace models
{
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
        /** Optional URL pointing to an emergency announcement web page */
        std::optional<std::string> emergencyUrl;
        /** The exit code parameters */
        std::optional<ExitCodeCheck> exitCode;
        /** URL of a help article */
        std::optional<std::string> helpUrl;
        /** URL of the error article */
        std::optional<std::string> errorFallbackUrl;

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
                return semver::version{latestVersion.value()};
            }
            catch (...) // couldn't convert version string
            {
                return semver::version{0, 0, 0};
            }
        }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        UpdateConfig,
        updatesDisabled,
        latestVersion,
        latestUrl,
        emergencyUrl,
        exitCode,
        helpUrl
    )    
}
