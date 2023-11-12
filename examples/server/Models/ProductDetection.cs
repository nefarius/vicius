using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;
using System.Text.Json.Serialization;

using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     The detection method of the installed software to use on the client.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[Newtonsoft.Json.JsonConverter(typeof(StringEnumConverter))]
public enum ProductVersionDetectionMethod
{
    /// <summary>
    ///     Performs checking a specified registry hive, key and value.
    /// </summary>
    [EnumMember(Value = nameof(RegistryValue))]
    RegistryValue,

    /// <summary>
    ///     Performs checking a specified file version resource.
    /// </summary>
    [EnumMember(Value = nameof(FileVersion))]
    FileVersion,

    /// <summary>
    ///     Performs checking a specified file size for matching.
    /// </summary>
    [EnumMember(Value = nameof(FileSize))]
    FileSize,

    /// <summary>
    ///     Calculates and compares the hash of a given file.
    /// </summary>
    [EnumMember(Value = nameof(FileChecksum))]
    FileChecksum,

    /// <summary>
    ///     Custom inja expression.
    /// </summary>
    [EnumMember(Value = nameof(CustomExpression))]
    CustomExpression
}

/// <summary>
///     Possible product version detection parameters.
/// </summary>
[JsonDerivedType(typeof(RegistryValueConfig), nameof(RegistryValueConfig))]
[KnownType(typeof(RegistryValueConfig))]
[JsonDerivedType(typeof(FileVersionConfig), nameof(FileVersionConfig))]
[KnownType(typeof(FileVersionConfig))]
[JsonDerivedType(typeof(FileSizeConfig), nameof(FileSizeConfig))]
[KnownType(typeof(FileSizeConfig))]
[JsonDerivedType(typeof(FileChecksumConfig), nameof(FileChecksumConfig))]
[KnownType(typeof(FileChecksumConfig))]
[JsonDerivedType(typeof(CustomExpressionConfig), nameof(CustomExpressionConfig))]
[KnownType(typeof(CustomExpressionConfig))]
public abstract class ProductVersionDetectionImplementation
{
}

/// <summary>
///     Queries a specific registry value and matches it against the selected <see cref="UpdateRelease" /> version.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class RegistryValueConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The alternate view.
    /// </summary>
    [Required]
    public RegistryView View { get; set; } = RegistryView.Default;

    /// <summary>
    ///     The hive.
    /// </summary>
    [Required]
    public required RegistryHive Hive { get; set; }

    /// <summary>
    ///     The (sub-)key.
    /// </summary>
    [Required]
    public required string Key { get; set; }

    /// <summary>
    ///     The value name.
    /// </summary>
    [Required]
    public required string Value { get; set; }
}

/// <summary>
///     Reads the version resource of the specified local file and matches it against the selected
///     <see cref="UpdateRelease" /> version.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class FileVersionConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The absolute local path to the file to read or an inja template.
    /// </summary>
    [Required]
    public required string Input { get; set; }

    /// <summary>
    ///     The <see cref="VersionResource" /> to read.
    /// </summary>
    public VersionResource Statement { get; set; } = VersionResource.PRODUCTVERSION;

    /// <summary>
    ///     Optional inja template data.
    /// </summary>
    public Dictionary<string, string>? Data { get; set; }
}

/// <summary>
///     Compares the size of a given file against the provided value.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class FileSizeConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The absolute local path to the file to read or an inja template.
    /// </summary>
    [Required]
    public required string Input { get; set; }

    /// <summary>
    ///     Optional inja template data.
    /// </summary>
    public Dictionary<string, string>? Data { get; set; }
}

/// <summary>
///     Calculates and compares the checksum/hash of a given file.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class FileChecksumConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The absolute local path to the file to read or an inja template.
    /// </summary>
    [Required]
    public required string Input { get; set; }

    /// <summary>
    ///     Optional inja template data.
    /// </summary>
    public Dictionary<string, string>? Data { get; set; }
}

/// <summary>
///     A custom expression to evaluate.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class CustomExpressionConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The inja template.
    /// </summary>
    [Required]
    public required string Input { get; set; }

    /// <summary>
    ///     Optional inja template data.
    /// </summary>
    public Dictionary<string, string>? Data { get; set; }
}