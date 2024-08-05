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

    using scp = SignatureComparisonPolicy;

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureComparisonPolicy, {
                                 {scp::Invalid, nullptr},
                                 {scp::Relaxed, magic_enum::enum_name(scp::Relaxed)},
                                 {scp::Strict, magic_enum::enum_name(scp::Strict)},
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

    using svs = SignatureVerificationStrategy;

    NLOHMANN_JSON_SERIALIZE_ENUM(SignatureVerificationStrategy, {
                                 {svs::Invalid, nullptr},
                                 {svs::FromUpdaterBinary, magic_enum::enum_name(svs::FromUpdaterBinary)},
                                 {svs::FromConfiguration, magic_enum::enum_name(svs::FromConfiguration)},
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

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SignatureConfig, issuerName, subjectName, serialNumber)
}
