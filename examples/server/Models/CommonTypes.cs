using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;

using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     The supported checksum/hashing algorithms.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum ChecksumAlgorithm
{
    /// <summary>
    ///     MD5.
    /// </summary>
    [EnumMember(Value = nameof(MD5))]
    MD5,

    /// <summary>
    ///     SHA1.
    /// </summary>
    [EnumMember(Value = nameof(SHA1))]
    SHA1,

    /// <summary>
    ///     SHA256.
    /// </summary>
    [EnumMember(Value = nameof(SHA256))]
    SHA256
}

/// <summary>
///     The registry hive to search for keys/values under.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum RegistryHive
{
    /// <summary>
    ///     HKEY_CURRENT_USER hive.
    /// </summary>
    [EnumMember(Value = nameof(HKCU))]
    HKCU,

    /// <summary>
    ///     HKEY_LOCAL_MACHINE hive.
    /// </summary>
    [EnumMember(Value = nameof(HKLM))]
    HKLM,

    /// <summary>
    ///     HKEY_CLASSES_ROOT hive.
    /// </summary>
    [EnumMember(Value = nameof(HKCR))]
    HKCR
}

/// <summary>
///     The alternate registry view to apply.
/// </summary>
/// <remarks>https://learn.microsoft.com/en-us/windows/win32/winprog64/accessing-an-alternate-registry-view</remarks>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum RegistryView
{
    /// <summary>
    ///     Default behavior (no flags set).
    /// </summary>
    [EnumMember(Value = nameof(Default))]
    Default,

    /// <summary>
    ///     Access a 64-bit key from either a 32-bit or 64-bit application.
    /// </summary>
    [EnumMember(Value = nameof(WOW64_64KEY))]
    WOW64_64KEY,

    /// <summary>
    ///     Access a 32-bit key from either a 32-bit or 64-bit application.
    /// </summary>
    [EnumMember(Value = nameof(WOW64_32KEY))]
    WOW64_32KEY
}

/// <summary>
///     Describes which version resource fixed-value to read.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum VersionResource
{
    /// <summary>
    ///     Binary version number for the file. The version consists of two 32-bit integers, defined by four 16-bit integers.
    ///     For example, "FILEVERSION 3,10,0,61" is translated into two doublewords: 0x0003000a and 0x0000003d, in that order.
    ///     Therefore, if version is defined by the DWORD values dw1 and dw2, they need to appear in the FILEVERSION statement
    ///     as follows: HIWORD(dw1), LOWORD(dw1), HIWORD(dw2), LOWORD(dw2).
    /// </summary>
    [EnumMember(Value = nameof(FILEVERSION))]
    FILEVERSION,

    /// <summary>
    ///     Binary version number for the product with which the file is distributed. The version parameter is two 32-bit
    ///     integers, defined by four 16-bit integers. For more information about version, see the FILEVERSION description.
    /// </summary>
    [EnumMember(Value = nameof(PRODUCTVERSION))]
    PRODUCTVERSION
}