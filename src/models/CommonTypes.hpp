#pragma once

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Possible hashing algorithms for file checksums.
     */
    enum class ChecksumAlgorithm
    {
        MD5,
        SHA1,
        SHA256,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ChecksumAlgorithm,
                                 {
                                   {ChecksumAlgorithm::Invalid, nullptr},
                                   {ChecksumAlgorithm::MD5, magic_enum::enum_name(ChecksumAlgorithm::MD5)},
                                   {ChecksumAlgorithm::SHA1, magic_enum::enum_name(ChecksumAlgorithm::SHA1)},
                                   {ChecksumAlgorithm::SHA256, magic_enum::enum_name(ChecksumAlgorithm::SHA256)},
                                 })

    /**
     * \brief Possible registry hives.
     */
    enum class RegistryHive
    {
        HKCU,
        HKLM,
        HKCR,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(RegistryHive, {
                                                 {RegistryHive::Invalid, nullptr},
                                                 {RegistryHive::HKCU, magic_enum::enum_name(RegistryHive::HKCU)},
                                                 {RegistryHive::HKLM, magic_enum::enum_name(RegistryHive::HKLM)},
                                                 {RegistryHive::HKCR, magic_enum::enum_name(RegistryHive::HKCR)},
                                               })

    /**
     * \brief https://learn.microsoft.com/en-us/windows/win32/winprog64/accessing-an-alternate-registry-view
     */
    enum class RegistryView
    {
        Default,
        WOW64_64KEY = 0x0100,
        WOW64_32KEY = 0x0200,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(RegistryView, {
                                                 {RegistryView::Invalid, nullptr},
                                                 {RegistryView::Default, magic_enum::enum_name(RegistryView::Default)},
                                                 {RegistryView::WOW64_64KEY, magic_enum::enum_name(RegistryView::WOW64_64KEY)},
                                                 {RegistryView::WOW64_32KEY, magic_enum::enum_name(RegistryView::WOW64_32KEY)},
                                               })

    /**
     * \brief VERSIONINFO resource fixed-info.
     */
    enum class VersionResource
    {
        FILEVERSION,
        PRODUCTVERSION,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(VersionResource,
                                 {
                                   {VersionResource::Invalid, nullptr},
                                   {VersionResource::FILEVERSION, magic_enum::enum_name(VersionResource::FILEVERSION)},
                                   {VersionResource::PRODUCTVERSION, magic_enum::enum_name(VersionResource::PRODUCTVERSION)},
                                 })
}
