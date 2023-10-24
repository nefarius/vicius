using System.ComponentModel.DataAnnotations;
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
///     The detection method of the installed software to use on the client.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
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
    FileChecksum
}

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum RegistryHive
{
    [EnumMember(Value = nameof(HKCU))]
    HKCU,

    [EnumMember(Value = nameof(HKLM))]
    HKLM,

    [EnumMember(Value = nameof(HKCR))]
    HKCR
}

[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class RegistryValueConfig
{
    [Required]
    public RegistryHive Hive { get; set; }

    [Required]
    public string Key { get; set; }

    [Required]
    public string Value { get; set; }
}

public sealed class FileVersionConfig
{
    [Required]
    public string Path { get; set; }

    [Required]
    public string Version { get; set; }
}

public sealed class FileSizeConfig
{
    [Required]
    public string Path { get; set; }

    [Required]
    public int Size { get; set; }
}

public sealed class FileChecksumConfig
{
    [Required]
    public string Path { get; set; }

    [Required]
    public ChecksumAlgorithm Algorithm { get; set; }

    [Required]
    public string Hash { get; set; }
}

/// <summary>
///     Parameters that might be provided by both the server and the local configuration.
/// </summary>
public sealed class SharedConfig
{
    public string? WindowTitle { get; set; }

    public string? ProductName { get; set; }

    public ProductVersionDetectionMethod? DetectionMethod { get; set; }

    public object? Detection { get; set; }
}

public sealed class MergedConfig
{
    [Required]
    public string WindowTitle { get; set; }

    [Required]
    public string ProductName { get; set; }

    [Required]
    public ProductVersionDetectionMethod DetectionMethod { get; set; }

    [Required]
    public object Detection { get; set; }
}

/// <summary>
///     Setup exit code parameters.
/// </summary>
public sealed class ExitCodeCheck
{
    [Required]
    public bool SkipCheck { get; set; }

    [Required]
    public List<int> SuccessCodes { get; set; } = new() { 0 };
}

public sealed class ChecksumParameters
{
    [Required]
    public string Checksum { get; set; }

    [Required]
    public ChecksumAlgorithm ChecksumAlg { get; set; }
}

/// <summary>
///     Represents an update release.
/// </summary>
public sealed class UpdateRelease
{
    [Required]
    public string Name { get; set; } = null!;

    [Required]
    public string Version { get; set; } = null!;

    [Required]
    public string Summary { get; set; } = null!;

    [Required]
    public string PublishedAt { get; set; } = null!;

    [Required]
    public string DownloadUrl { get; set; } = null!;

    public int? DownloadSize { get; set; }

    public string? LaunchArguments { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    public ExitCodeCheck? ExitCode { get; set; }
}

/// <summary>
///     Update instance configuration. Parameters applying to the entire product/tenant.
/// </summary>
public sealed class UpdateConfig
{
    public bool? UpdatesDisabled { get; set; }

    public string? LatestVersion { get; set; }

    public string? LatestUrl { get; set; }

    public string? EmergencyUrl { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    public ExitCodeCheck? ExitCode { get; set; }
}

/// <summary>
///     An instance returned by the remote update API.
/// </summary>
public sealed class UpdateResponse
{
    /// <summary>
    ///     Update instance configuration. Parameters applying to the entire product/tenant.
    /// </summary>
    [Required]
    public UpdateConfig Instance { get; set; } = new();

    /// <summary>
    ///     Parameters that might be provided by both the server and the local configuration.
    /// </summary>
    public SharedConfig? Shared { get; set; }

    /// <summary>
    ///     The available update releases.
    /// </summary>
    [Required]
    public List<UpdateRelease> Releases { get; set; } = new();
}