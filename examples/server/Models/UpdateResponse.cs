using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;
using System.Text.Json.Serialization;

using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     The supported checksum/hashing algorithms.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[Newtonsoft.Json.JsonConverter(typeof(StringEnumConverter))]
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
    FileChecksum
}

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[Newtonsoft.Json.JsonConverter(typeof(StringEnumConverter))]
public enum RegistryHive
{
    [EnumMember(Value = nameof(HKCU))]
    HKCU,

    [EnumMember(Value = nameof(HKLM))]
    HKLM,

    [EnumMember(Value = nameof(HKCR))]
    HKCR
}

[JsonDerivedType(typeof(RegistryValueConfig), typeDiscriminator: nameof(RegistryValueConfig))]
[JsonDerivedType(typeof(FileVersionConfig), typeDiscriminator: nameof(FileVersionConfig))]
[JsonDerivedType(typeof(FileSizeConfig), typeDiscriminator: nameof(FileSizeConfig))]
[JsonDerivedType(typeof(FileChecksumConfig), typeDiscriminator: nameof(FileChecksumConfig))]
public abstract class ProductVersionDetectionImplementation
{
}

[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class RegistryValueConfig : ProductVersionDetectionImplementation
{
    [Required]
    public RegistryHive Hive { get; set; }

    [Required]
    public string Key { get; set; }

    [Required]
    public string Value { get; set; }
}

public sealed class FileVersionConfig : ProductVersionDetectionImplementation
{
    [Required]
    public string Path { get; set; }

    [Required]
    public string Version { get; set; }
}

public sealed class FileSizeConfig : ProductVersionDetectionImplementation
{
    [Required]
    public string Path { get; set; }

    [Required]
    public int Size { get; set; }
}

public sealed class FileChecksumConfig : ProductVersionDetectionImplementation
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

    public ProductVersionDetectionImplementation? Detection { get; set; }
}

/// <summary>
///     The shared configuration that has been merged with local and remote parameters.
/// </summary>
public sealed class MergedConfig
{
    /// <summary>
    ///     The process window title visible in the taskbar.
    /// </summary>
    [Required]
    public required string WindowTitle { get; set; }

    [Required]
    public required string ProductName { get; set; }

    [Required]
    public required ProductVersionDetectionMethod DetectionMethod { get; set; }

    [Required]
    public required ProductVersionDetectionImplementation Detection { get; set; }
}

/// <summary>
///     Setup exit code parameters.
/// </summary>
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
[SuppressMessage("ReSharper", "CollectionNeverQueried.Global")]
public sealed class ExitCodeCheck
{
    /// <summary>
    ///     Ignore whatever exit code we got if true. 
    /// </summary>
    [Required]
    public bool SkipCheck { get; set; }

    /// <summary>
    ///     One or more exit codes that signify success.
    /// </summary>
    [Required]
    public List<int> SuccessCodes { get; set; } = new();
}

/// <summary>
///     Parameters for checksum/hash calculation.
/// </summary>
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class ChecksumParameters
{
    /// <summary>
    ///     The checksum/hash value to compare against.
    /// </summary>
    [Required]
    public string Checksum { get; set; } = null!;

    /// <summary>
    ///     The algorithm to use to calculate the checksum/hash.
    /// </summary>
    [Required]
    public ChecksumAlgorithm ChecksumAlg { get; set; }
}

/// <summary>
///     Represents an update release.
/// </summary>
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class UpdateRelease
{
    /// <summary>
    ///     Simple display name of the release.
    /// </summary>
    [Required]
    public string Name { get; set; } = null!;

    /// <summary>
    ///     Semantic version of the release.
    /// </summary>
    [Required]
    public Version Version { get; set; } = null!;

    /// <summary>
    ///     Summary/changelog/description of the release. Supports Markdown syntax.
    /// </summary>
    [Required]
    public string Summary { get; set; } = null!;

    /// <summary>
    ///     The release publish timestamp.
    /// </summary>
    /// <remarks>
    ///     Releases are sorted on the client by descending <see cref="Version" />, even if the publish date doesn't match the
    ///     order of the release.
    /// </remarks>
    [Required]
    public DateTimeOffset PublishedAt { get; set; }

    /// <summary>
    ///     The direct URL to the new product setup.
    /// </summary>
    [Required]
    public string DownloadUrl { get; set; } = null!;

    /// <summary>
    ///     Optional size (in bytes) of the download target.
    /// </summary>
    public long? DownloadSize { get; set; }

    /// <summary>
    ///     Optional arguments to pass to the setup process.
    /// </summary>
    public string? LaunchArguments { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    public ExitCodeCheck? ExitCode { get; set; }

    /// <summary>
    ///     Optional checksum/hashing settings to perform after download.
    /// </summary>
    public ChecksumParameters? Checksum { get; set; }

    /// <summary>
    ///     Skips/disables this release on the client if set.
    /// </summary>
    public bool? Disabled { get; set; }
}

/// <summary>
///     Update instance configuration. Parameters applying to the entire product/tenant.
/// </summary>
public sealed class UpdateConfig
{
    /// <summary>
    ///     Updates are currently disabled, do not do anything, even if new versions are found.
    /// </summary>
    public bool? UpdatesDisabled { get; set; }

    /// <summary>
    ///     The latest version of the updater binary.
    /// </summary>
    public string? LatestVersion { get; set; }

    /// <summary>
    ///     Direct URL to the latest updater binary.
    /// </summary>
    public string? LatestUrl { get; set; }

    /// <summary>
    ///     The emergency URL. See https://docs.nefarius.at/projects/Vicius/Emergency-Feature/
    /// </summary>
    public string? EmergencyUrl { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    public ExitCodeCheck? ExitCode { get; set; }
}

/// <summary>
///     An instance returned by the remote update API.
/// </summary>
[SuppressMessage("ReSharper", "CollectionNeverQueried.Global")]
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