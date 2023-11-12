#pragma once
#include "Common.h"
#include "ADL.hpp"
#include "CommonTypes.hpp"
#include "SignatureValidation.hpp"
#include "SharedConfig.hpp"

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Setup exit code parameters.
     */
    class ExitCodeCheck
    {
    public:
        /** True to skip exit code check */
        bool skipCheck{false};
        /** The setup exit codes treated as success */
        std::vector<int> successCodes{0};
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ExitCodeCheck, skipCheck, successCodes)

    /**
     * \brief Details about checksum/hash calculation.
     */
    class ChecksumParameters
    {
    public:
        /** The checksum to compare against */
        std::string checksum;
        /** The checksum algorithm */
        ChecksumAlgorithm checksumAlg;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ChecksumParameters, checksum, checksumAlg)

    /**
     * \brief Represents an update release.
     */
    class UpdateRelease
    {
    public:
        /** The release display name */
        std::string name;
        /** The version as a 3-digit SemVer (1.0.0) or 4-digit (1.0.0.0) string */
        std::string version;
        /** The update summary/changelog, supports Markdown */
        std::string summary;
        /** The publishing timestamp as UTC ISO 8601 string */
        std::string publishedAt;
        /** URL of the new setup/release download */
        std::string downloadUrl;
        /** Size of the remote file in bytes */
        std::optional<size_t> downloadSize;
        /** The launch arguments (CLI arguments) if any */
        std::optional<std::string> launchArguments;
        /** The exit code parameters */
        std::optional<ExitCodeCheck> exitCode;
        /** The (optional) checksum of the remote setup */
        std::optional<ChecksumParameters> checksum;
        /** If set, this release is ignored and not presented to the user */
        std::optional<bool> disabled;
        /** The file hash to use in product detection */
        std::optional<ChecksumParameters> detectionChecksum;
        /** Size of the remote file in bytes */
        std::optional<size_t> detectionSize;
        /** The version to use in product detection */
        std::optional<std::string> detectionVersion;

        /** Full pathname of the local temporary file */
        std::filesystem::path localTempFilePath{};

        /**
         * \brief Converts the version string to a SemVer type.
         * \return The parsed version.
         */
        semver::version GetSemVersion() const
        {
            try
            {
                // trim whitespaces and potential "v" prefix
                return semver::version{util::trim(version, "v \t")};
            }
            catch (...)
            {
                return semver::version{0, 0, 0};
            }
        }

        /**
         * \brief Gets the detection version, if provided.
         * \return The parsed version.
         */
        std::optional<semver::version> GetDetectionSemVersion() const
        {
            if (!detectionVersion.has_value())
            {
                // fallback value is mandatory
                return GetSemVersion();
            }

            try
            {
                // trim whitespaces and potential "v" prefix
                return semver::version{util::trim(detectionVersion.value(), "v \t")};
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        UpdateRelease,
        name,
        version,
        summary,
        publishedAt,
        downloadUrl,
        downloadSize,
        launchArguments,
        exitCode,
        checksum,
        disabled,
        detectionChecksum,
        detectionSize,
        detectionVersion
    )

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

    /**
     * \brief An instance returned by the remote update API.
     */
    class UpdateResponse
    {
    public:
        /** The (optional) global settings instance */
        std::optional<UpdateConfig> instance;
        /** The (optional) shared settings */
        std::optional<SharedConfig> shared;
        /** The available releases */
        std::vector<UpdateRelease> releases;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        UpdateResponse,
        instance,
        shared,
        releases
    )
}
