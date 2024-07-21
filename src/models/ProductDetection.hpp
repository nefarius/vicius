#pragma once

#include "CommonTypes.hpp"

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Possible installed product detection mechanisms.
     */
    enum class ProductVersionDetectionMethod
    {
        RegistryValue,
        FileVersion,
        FileSize,
        FileChecksum,
        CustomExpression,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(
      ProductVersionDetectionMethod,
      {
        {ProductVersionDetectionMethod::Invalid, nullptr},
        {ProductVersionDetectionMethod::RegistryValue, magic_enum::enum_name(ProductVersionDetectionMethod::RegistryValue)},
        {ProductVersionDetectionMethod::FileVersion, magic_enum::enum_name(ProductVersionDetectionMethod::FileVersion)},
        {ProductVersionDetectionMethod::FileSize, magic_enum::enum_name(ProductVersionDetectionMethod::FileSize)},
        {ProductVersionDetectionMethod::FileChecksum, magic_enum::enum_name(ProductVersionDetectionMethod::FileChecksum)},
        {ProductVersionDetectionMethod::CustomExpression, magic_enum::enum_name(ProductVersionDetectionMethod::CustomExpression)},
      })

    /**
     * \brief Parameters for querying the registry.
     */
    class RegistryValueConfig
    {
    public:
        RegistryView view{RegistryView::Default};
        RegistryHive hive;
        std::string key;
        std::string value;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RegistryValueConfig, view, hive, key, value)

    /**
     * \brief Parameters for reading a file version resource.
     */
    class FileVersionConfig
    {
    public:
        /** The path or inja template */
        std::string input;
        /** The statement */
        VersionResource statement{VersionResource::PRODUCTVERSION};
        /** The optional inja template data */
        std::optional<json> data;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FileVersionConfig, input, statement, data)

    /**
     * \brief Parameters for reading a file size.
     */
    class FileSizeConfig
    {
    public:
        /** The path or inja template */
        std::string input;
        /** The optional inja template data */
        std::optional<json> data;
        /** The size to compare to is taken from the selected release */
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FileSizeConfig, input, data)

    /**
     * \brief Parameters for hashing a file.
     */
    class FileChecksumConfig
    {
    public:
        /** The path or inja template */
        std::string input;
        /** The optional inja template data */
        std::optional<json> data;
        /** The algorithm and hash to compare to is taken from the selected release */
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FileChecksumConfig, input, data)

    /**
     * \brief A custom expression to evaluate.
     */
    class CustomExpressionConfig
    {
    public:
        /** The inja template */
        std::string input;
        /** The optional inja template data */
        std::optional<json> data;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CustomExpressionConfig, input, data)
}
