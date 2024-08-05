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

    using ca = ChecksumAlgorithm;

    NLOHMANN_JSON_SERIALIZE_ENUM(ChecksumAlgorithm, {
                                 {ca::Invalid, nullptr},
                                 {ca::MD5, magic_enum::enum_name(ca::MD5)},
                                 {ca::SHA1, magic_enum::enum_name(ca::SHA1)},
                                 {ca::SHA256, magic_enum::enum_name(ca::SHA256)},
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

    using rh = RegistryHive;

    NLOHMANN_JSON_SERIALIZE_ENUM(RegistryHive, {
                                 {rh::Invalid, nullptr},
                                 {rh::HKCU, magic_enum::enum_name(rh::HKCU)},
                                 {rh::HKLM, magic_enum::enum_name(rh::HKLM)},
                                 {rh::HKCR, magic_enum::enum_name(rh::HKCR)},
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

    using rv = RegistryView;

    NLOHMANN_JSON_SERIALIZE_ENUM(RegistryView, {
                                 {rv::Invalid, nullptr},
                                 {rv::Default, magic_enum::enum_name(rv::Default)},
                                 {rv::WOW64_64KEY, magic_enum::enum_name(rv::WOW64_64KEY)},
                                 {rv::WOW64_32KEY, magic_enum::enum_name(rv::WOW64_32KEY)},
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

    using vr = VersionResource;

    NLOHMANN_JSON_SERIALIZE_ENUM(VersionResource, {
                                 {vr::Invalid, nullptr},
                                 {vr::FILEVERSION, magic_enum::enum_name(vr::FILEVERSION)},
                                 {vr::PRODUCTVERSION, magic_enum::enum_name(vr::PRODUCTVERSION)},
                                 })
}
