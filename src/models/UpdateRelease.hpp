#pragma once

#include "CommonTypes.hpp"
#include "SignatureValidation.hpp"
#include "../ADL.hpp"

namespace models
{
    /**
     * \brief Per-exit-code UI message entry.
     */
    class ExitCodeMessage
    {
    public:
        /** The message displayed in the UI for this exit code */
        std::string message;
        /** When true, this exit code is treated as a success condition even if absent from successCodes */
        std::optional<bool> isSuccess;
        /** Optional URL opened by the help/more-info button shown alongside the message */
        std::optional<std::string> helpUrl;
        /** Optional custom label for the help/more-info button (defaults to "Open help page" if omitted) */
        std::optional<std::string> buttonText;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ExitCodeMessage, message, isSuccess, helpUrl, buttonText)

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
        /** Optional per-exit-code UI messages, keyed by decimal exit code string (e.g. "3010") */
        std::optional<std::unordered_map<std::string, ExitCodeMessage>> messages;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ExitCodeCheck, skipCheck, successCodes, messages)

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
     * \brief How to treat files when installing a .zip file
     */
    enum class ZipExtractFileDisposition
    {
        CreateIfAbsent,
        CreateOrReplace,
        DeleteIfPresent,
    };

    using zefd = ZipExtractFileDisposition;

    NLOHMANN_JSON_SERIALIZE_ENUM(ZipExtractFileDisposition, {
                                 {zefd::CreateIfAbsent, magic_enum::enum_name(zefd::CreateIfAbsent)},
                                 {zefd::CreateOrReplace, magic_enum::enum_name(zefd::CreateOrReplace)},
                                 {zefd::DeleteIfPresent, magic_enum::enum_name(zefd::DeleteIfPresent)}
                                 })

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
        /**
         * \brief Optional list of mirror/alternative download URLs tried in order
         * when \c downloadUrl is unreachable.  All URLs must use HTTPS.
         * Useful when the primary CDN host is blocked by a firewall or censorship.
         */
        std::optional<std::vector<std::string>> mirrorUrls;
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
        /**
         * \brief Optional Authenticode certificate pin for this release.
         * When present, the downloaded setup's signing cert is compared against these values.
         * Implies signatureStrategy = FromConfiguration.
         * Serial/thumbprint fields here are safe because they travel inside the signed manifest.
         */
        std::optional<SignatureConfig> signature;
        /** Override the global Authenticode verification policy for this specific release */
        std::optional<SignatureComparisonPolicy> signaturePolicy;
        /** Override the global Authenticode pin strategy for this specific release */
        std::optional<SignatureVerificationStrategy> signatureStrategy;
        /** The file hash to use in product detection */
        std::optional<ChecksumParameters> detectionChecksum;
        /** Size of the remote file in bytes */
        std::optional<size_t> detectionSize;
        /** The version to use in product detection */
        std::optional<std::string> detectionVersion;
        /** How to handle files in a zip update, unless overriden */
        std::optional<ZipExtractFileDisposition> zipExtractDefaultFileDisposition;
        /** Override the behavior for specific files in a zip update */
        std::optional<std::unordered_map<std::string, ZipExtractFileDisposition>> zipExtractFileDispositionOverrides;
        /** If true, the downloaded setup is launched elevated (As Administrator) via the "runas" verb */
        std::optional<bool> runAsAdmin;

        /** Full pathname of the local temporary file */
        std::filesystem::path localTempFilePath{};


        /**
         * \brief Converts the version string to a SemVer type.
         * \return The parsed version.
         */
        semver::version GetSemVersion() const;

        /**
         * \brief Gets the detection version, if provided.
         * \return The parsed version.
         */
        std::optional<semver::version> GetDetectionSemVersion() const;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateRelease,
                                                    name,
                                                    version,
                                                    summary,
                                                    publishedAt,
                                                    downloadUrl,
                                                    mirrorUrls,
                                                    downloadSize,
                                                    launchArguments,
                                                    exitCode,
                                                    checksum,
                                                    disabled,
                                                    signature,
                                                    signaturePolicy,
                                                    signatureStrategy,
                                                    detectionChecksum,
                                                    detectionSize,
                                                    detectionVersion,
                                                    zipExtractDefaultFileDisposition,
                                                    zipExtractFileDispositionOverrides,
                                                    runAsAdmin)
}
