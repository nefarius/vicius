using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

using NJsonSchema.Annotations;

namespace Nefarius.Vicius.Abstractions.Models;

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
    public List<int> SuccessCodes { get; } = new();
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
    public required string Name { get; set; } = null!;

    /// <summary>
    ///     Semantic version of the release.
    /// </summary>
    [Required]
    [JsonSchemaType(typeof(string))]
    public required Version Version { get; set; } = null!;

    /// <summary>
    ///     Summary/changelog/description of the release. Supports Markdown syntax.
    /// </summary>
    [Required]
    public required string Summary { get; set; } = null!;

    /// <summary>
    ///     The release publish timestamp.
    /// </summary>
    /// <remarks>
    ///     Releases are sorted on the client by descending <see cref="Version" />, even if the publish date doesn't match the
    ///     order of the release.
    /// </remarks>
    [Required]
    public required DateTimeOffset PublishedAt { get; set; }

    /// <summary>
    ///     The URL to the new product setup. Redirects are supported.
    /// </summary>
    [Required]
    public required string DownloadUrl { get; set; } = null!;

    /// <summary>
    ///     Optional size (in bytes) of the download target. If this is not set, the UI will display "N/A" until download
    ///     starts.
    /// </summary>
    public long? DownloadSize { get; set; }

    /// <summary>
    ///     Optional arguments to pass to the setup process.
    /// </summary>
    public string? LaunchArguments { get; set; }

    /// <summary>
    ///     Setup exit code parameters.
    /// </summary>
    /// <remarks>You can use <see cref="UpdateConfig.ExitCode" /> instead to apply to all releases.</remarks>
    public ExitCodeCheck? ExitCode { get; set; }

    /// <summary>
    ///     Optional checksum/hashing settings to perform after download.
    /// </summary>
    /// <remarks>
    ///     This simply hashes the file contents and compares them; there is no cryptographic signature validation
    ///     performed. It can be used to ensure that the file hasn't been corrupted in transit but doesn't say anything about
    ///     the authenticity or origin of the file.
    /// </remarks>
    public ChecksumParameters? Checksum { get; set; }

    /// <summary>
    ///     Skips/disables this release on the client if set.
    /// </summary>
    public bool? Disabled { get; set; }

    /// <summary>
    ///     Optional per-release Authenticode certificate pin.
    ///     When present, the downloaded setup's signing cert is compared against these values.
    ///     Implies <see cref="SignatureVerificationStrategy.FromConfiguration" />.
    /// </summary>
    public SignatureConfig? Signature { get; set; }

    /// <summary>
    ///     Override the global Authenticode verification policy for this specific release.
    /// </summary>
    public SignatureComparisonPolicy? SignaturePolicy { get; set; }

    /// <summary>
    ///     Override the global Authenticode pin strategy for this specific release.
    /// </summary>
    public SignatureVerificationStrategy? SignatureStrategy { get; set; }

    /// <summary>
    ///     The file hash to use in product detection. This can differ from <see cref="Checksum" />.
    /// </summary>
    public ChecksumParameters? DetectionChecksum { get; set; }

    /// <summary>
    ///     The size value (in bytes) used in product detection. This can differ from <see cref="DownloadSize" />.
    /// </summary>
    public long? DetectionSize { get; set; }

    /// <summary>
    ///     The version used in product detection. This can differ from <see cref="Version" />.
    /// </summary>
    [JsonSchemaType(typeof(string))]
    public Version? DetectionVersion { get; set; }

    /// <summary>
    ///     Specifies how to handle files in a zip update, unless overridden via
    ///     <see cref="ZipExtractFileDispositionOverrides" />.
    /// </summary>
    public ZipExtractFileDisposition? ZipExtractDefaultFileDisposition { get; set; }

    /// <summary>
    ///     Overrides the behavior for specific files in a zip update. Trumps
    ///     <see cref="ZipExtractDefaultFileDisposition" /> if set.
    /// </summary>
    public Dictionary<string, ZipExtractFileDisposition>? ZipExtractFileDispositionOverrides { get; set; }

    /// <summary>
    ///     When true, the downloaded setup is launched elevated (As Administrator) via the "runas" verb.
    /// </summary>
    /// <remarks>
    ///     Only honoured when strong authenticity conditions are satisfied (signature verification mode is
    ///     <see cref="SignatureVerificationMode.Required" /> or --strict-verification is active). Ignored otherwise
    ///     to prevent a compromised server from triggering an unauthenticated UAC prompt.
    /// </remarks>
    public bool? RunAsAdmin { get; set; }
}
