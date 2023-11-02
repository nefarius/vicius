using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;
using System.Text.Json.Serialization;

using Newtonsoft.Json.Converters;

using NJsonSchema.Annotations;

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

/// <summary>
///     The registry hive to search for keys/values under.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[Newtonsoft.Json.JsonConverter(typeof(StringEnumConverter))]
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
[Newtonsoft.Json.JsonConverter(typeof(StringEnumConverter))]
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
///     Possible product version detection parameters.
/// </summary>
[JsonDerivedType(typeof(RegistryValueConfig), nameof(RegistryValueConfig))]
[JsonDerivedType(typeof(FileVersionConfig), nameof(FileVersionConfig))]
[JsonDerivedType(typeof(FileSizeConfig), nameof(FileSizeConfig))]
[JsonDerivedType(typeof(FileChecksumConfig), nameof(FileChecksumConfig))]
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
    ///     The absolute local path to the file to read.
    /// </summary>
    [Required]
    public required string Path { get; set; }
}

/// <summary>
///     Compares the size of a given file against the provided value.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class FileSizeConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The absolute local path to the file to read.
    /// </summary>
    [Required]
    public required string Path { get; set; }

    /// <summary>
    ///     The expected file size in bytes. If the file versions do not match, the product is flagged as outdated.
    /// </summary>
    [Required]
    public required long Size { get; set; }
}

/// <summary>
///     Calculates and compares the checksum/hash of a given file.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class FileChecksumConfig : ProductVersionDetectionImplementation
{
    /// <summary>
    ///     The absolute local path to the file to read.
    /// </summary>
    [Required]
    public required string Path { get; set; }

    /// <summary>
    ///     The hashing algorithm to use.
    /// </summary>
    [Required]
    public required ChecksumAlgorithm Algorithm { get; set; }

    /// <summary>
    ///     The expected hash string. If the hashes do not match, the product is flagged as outdated.
    /// </summary>
    [Required]
    public required string Hash { get; set; }
}

/// <summary>
///     Parameters that might be provided by both the server and the local configuration.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
public sealed class SharedConfig
{
    /// <summary>
    ///     The process window title visible in the taskbar.
    /// </summary>
    public string? WindowTitle { get; set; }

    /// <summary>
    ///     The product name displayed in the UI and dialogs.
    /// </summary>
    public string? ProductName { get; set; }

    /// <summary>
    ///     The implementation to use to detect the locally installed product version to compare release versions against.
    /// </summary>
    public ProductVersionDetectionMethod? DetectionMethod { get; set; }

    /// <summary>
    ///     The details of the selected <see cref="DetectionMethod" />.
    /// </summary>
    public ProductVersionDetectionImplementation? Detection { get; set; }
}

/// <summary>
///     The shared configuration that has been merged with local and remote parameters.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class MergedConfig
{
    /// <summary>
    ///     The process window title visible in the taskbar.
    /// </summary>
    [Required]
    public required string WindowTitle { get; set; }

    /// <summary>
    ///     The product name displayed in the UI and dialogs.
    /// </summary>
    [Required]
    public required string ProductName { get; set; }

    /// <summary>
    ///     The implementation to use to detect the locally installed product version to compare release versions against.
    /// </summary>
    [Required]
    public required ProductVersionDetectionMethod DetectionMethod { get; set; }

    /// <summary>
    ///     The details of the selected <see cref="DetectionMethod" />.
    /// </summary>
    [Required]
    public required ProductVersionDetectionImplementation Detection { get; set; }
}

/// <summary>
///     Setup exit code parameters.
/// </summary>
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
[SuppressMessage("ReSharper", "CollectionNeverQueried.Global")]
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class ExitCodeCheck
{
    /// <summary>
    ///     Ignore whatever exit code we got if true.
    /// </summary>
    [Required]
    public bool SkipCheck { get; set; } = false;

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
[SuppressMessage("ReSharper", "UnusedMember.Global")]
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
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
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
    [JsonSchemaType(typeof(string))]
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
    ///     Optional size (in bytes) of the download target. If this is not set, the UI will simply display "N/A" until the
    ///     actual download starts.
    /// </summary>
    public long? DownloadSize { get; set; }

    /// <summary>
    ///     Optional arguments to pass to the setup process.
    /// </summary>
    public string? LaunchArguments { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    /// <remarks>You can use <see cref="UpdateConfig.ExitCode"/> instead to apply to all releases.</remarks>
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
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class UpdateConfig
{
    /// <summary>
    ///     Updates are currently disabled, do not do anything, even if new versions are found.
    /// </summary>
    public bool? UpdatesDisabled { get; set; }

    /// <summary>
    ///     The latest version of the updater binary.
    /// </summary>
    [JsonSchemaType(typeof(string))]
    public Version? LatestVersion { get; set; }

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

    /// <summary>
    ///     URL pointing to a help article opening when the user clicks the help button.
    /// </summary>
    public string? HelpUrl { get; set; }
}

/// <summary>
///     An instance returned by the remote update API.
/// </summary>
[SuppressMessage("ReSharper", "CollectionNeverQueried.Global")]
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
public sealed class UpdateResponse
{
    /// <summary>
    ///     Update instance configuration. Parameters applying to the entire product/tenant.
    /// </summary>
    public UpdateConfig? Instance { get; set; }

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