using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

using NJsonSchema.Annotations;

namespace Nefarius.Vicius.Example.Server.Models;

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public enum ChecksumAlgorithm
{
    MD5,
    SHA1,
    SHA256
}

[SuppressMessage("ReSharper", "UnusedMember.Global")]
public enum ProductVersionDetectionMethod
{
    RegistryValue,
    FileVersion,
    FileSize,
    FileChecksum
}

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public enum RegistryHive
{
    HKCU,
    HKLM,
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

public sealed class SharedConfig
{
    [CanBeNull]
    public string? WindowTitle { get; set; }

    [CanBeNull]
    public string? ProductName { get; set; }

    [CanBeNull]
    public ProductVersionDetectionMethod? DetectionMethod { get; set; }

    [CanBeNull]
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

    [CanBeNull]
    public int? DownloadSize { get; set; }

    [CanBeNull]
    public string? LaunchArguments { get; set; }

    [CanBeNull]
    public ExitCodeCheck? ExitCode { get; set; }
}

public sealed class UpdateConfig
{
    [CanBeNull]
    public bool? UpdatesDisabled { get; set; }

    [CanBeNull]
    public string? LatestVersion { get; set; }

    [CanBeNull]
    public string? LatestUrl { get; set; }

    [CanBeNull]
    public string? EmergencyUrl { get; set; }

    [CanBeNull]
    public ExitCodeCheck? ExitCode { get; set; }
}

public sealed class UpdateResponse
{
    [Required]
    public UpdateConfig Instance { get; set; } = new();

    [CanBeNull]
    public SharedConfig? Shared { get; set; }

    [Required]
    public List<UpdateRelease> Releases { get; set; } = new();
}