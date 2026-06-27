#pragma once

using json = nlohmann::json;

namespace models
{
    /**
     * \brief The policy to abide by when comparing signatures.
     *
     * Relaxed: a valid, trusted Authenticode chain is sufficient.
     * Strict:  the chain must be valid AND the cert identity must match the configured pin.
     */
    enum class SignatureComparisonPolicy
    {
        Relaxed,
        Strict,
        Invalid = -1
    };

    using scp = SignatureComparisonPolicy;

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureComparisonPolicy, {
                                 {scp::Invalid, nullptr},
                                 {scp::Relaxed, magic_enum::enum_name(scp::Relaxed)},
                                 {scp::Strict, magic_enum::enum_name(scp::Strict)},
                                 })

    /**
     * \brief The strategy on how to verify if the setup signature is trusted.
     *
     * FromUpdaterBinary: pin on the updater exe's own signing cert (subjectName only - renewal-safe).
     * FromConfiguration: pin on explicit values from the signed manifest / local config.
     */
    enum class SignatureVerificationStrategy
    {
        FromUpdaterBinary,
        FromConfiguration,
        Invalid = -1
    };

    using svs = SignatureVerificationStrategy;

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureVerificationStrategy, {
                                 {svs::Invalid, nullptr},
                                 {svs::FromUpdaterBinary, magic_enum::enum_name(svs::FromUpdaterBinary)},
                                 {svs::FromConfiguration, magic_enum::enum_name(svs::FromConfiguration)},
                                 })

    /**
     * \brief Controls whether signature verification of the setup binary is enforced.
     *
     * Disabled:    no Authenticode check is performed.
     * WhenPresent: check only if the setup is signed; unsigned setups are allowed (default).
     * Required:    a valid Authenticode chain is mandatory; unsigned setups are rejected.
     */
    enum class SignatureVerificationMode
    {
        Disabled,
        WhenPresent,
        Required,
        Invalid = -1
    };

    using svm = SignatureVerificationMode;

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureVerificationMode, {
                                 {svm::Invalid, nullptr},
                                 {svm::Disabled, magic_enum::enum_name(svm::Disabled)},
                                 {svm::WhenPresent, magic_enum::enum_name(svm::WhenPresent)},
                                 {svm::Required, magic_enum::enum_name(svm::Required)},
                                 })

    /**
     * \brief Authenticode certificate pin details.
     *
     * Cert fields by renewal stability:
     *   Stable  : subjectName  (tied to the legal entity - default pin for FromUpdaterBinary)
     *   Volatile: serialNumber, thumbprintSha1, thumbprintSha256 (change on every renewal)
     *   Semi-stable: issuerName (changes when CA rotates intermediates or you switch CA)
     *
     * Serial / thumbprint pinning is ONLY sound when these values travel inside the signed manifest
     * (FromConfiguration) so they can be rotated on cert renewal without redeploying the updater.
     */
    class SignatureConfig
    {
    public:
        /** CA / issuer display name (semi-stable - use with caution across renewals) */
        std::optional<std::string> issuerName;
        /** Subject / organization name - stable across renewals; default FromUpdaterBinary pin field */
        std::optional<std::string> subjectName;
        /** Certificate serial number - volatile; only safe via signed manifest (FromConfiguration) */
        std::optional<std::string> serialNumber;
        /** SHA-1 thumbprint (hex) - volatile; only safe via signed manifest (FromConfiguration) */
        std::optional<std::string> thumbprintSha1;
        /** SHA-256 thumbprint (hex) - volatile; only safe via signed manifest (FromConfiguration) */
        std::optional<std::string> thumbprintSha256;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SignatureConfig,
                                                    issuerName,
                                                    subjectName,
                                                    serialNumber,
                                                    thumbprintSha1,
                                                    thumbprintSha256)
}
