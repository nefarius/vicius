#pragma once

using json = nlohmann::json;

namespace models
{
    /**
     * \brief The policy to abide by when comparing signatures.
     */
    enum class SignatureComparisonPolicy
    {
        Relaxed,
        Strict,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureComparisonPolicy, {
                                 {SignatureComparisonPolicy::Invalid, nullptr},
                                 {SignatureComparisonPolicy::Relaxed,
                                 magic_enum::enum_name(SignatureComparisonPolicy::Relaxed)},
                                 {SignatureComparisonPolicy::Strict,
                                 magic_enum::enum_name(SignatureComparisonPolicy::Strict)},
                                 })

    /**
     * \brief The strategy on how to verify if the setup signature is trusted.
     */
    enum class SignatureVerificationStrategy
    {
        FromUpdaterBinary,
        FromConfiguration,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureVerificationStrategy, {
                                 {SignatureVerificationStrategy::Invalid, nullptr},
                                 {SignatureVerificationStrategy::FromUpdaterBinary,
                                 magic_enum::enum_name(SignatureVerificationStrategy::FromUpdaterBinary)},
                                 {SignatureVerificationStrategy::FromConfiguration,
                                 magic_enum::enum_name(SignatureVerificationStrategy::FromConfiguration)},
                                 })

    /**
     * \brief Authenticode signature details.
     */
    class SignatureConfig
    {
    public:
        std::optional<std::string> issuerName;
        std::optional<std::string> subjectName;
        std::optional<std::string> serialNumber;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        SignatureConfig,
        issuerName,
        subjectName,
        serialNumber
    )
}
