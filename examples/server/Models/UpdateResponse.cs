using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

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
    public RegistryHive Hive { get; set; }

    public string Key { get; set; }

    public string Value { get; set; }
}

public sealed class FileVersionConfig
{
    public string Path { get; set; }

    public string Version { get; set; }
}

public sealed class FileSizeConfig
{
    public string Path { get; set; }

    public int Size { get; set; }
}

public sealed class FileChecksumConfig
{
    public string Path { get; set; }

    public ChecksumAlgorithm Algorithm { get; set; }

    public string Hash { get; set; }
}

public sealed class SharedConfig
{
    public string? WindowTitle { get; set; }

    public string? ProductName { get; set; }

    public ProductVersionDetectionMethod? DetectionMethod { get; set; }

    public object? Detection { get; set; }
}

public sealed class MergedConfig
{
    public string WindowTitle { get; set; }

    public string ProductName { get; set; }

    public ProductVersionDetectionMethod DetectionMethod { get; set; }

    public object Detection { get; set; }
}

public sealed class ExitCodeCheck
{
    public bool SkipCheck { get; set; }

    public List<int> SuccessCodes { get; set; }
}

public sealed class ChecksumParameters
{
    public string Checksum { get; set; }

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

    public int? DownloadSize { get; set; }

    public string? LaunchArguments { get; set; }

    public ExitCodeCheck? ExitCode { get; set; }
}

public sealed class UpdateConfig
{
    public bool? UpdatesDisabled { get; set; }

    public string? LatestVersion { get; set; }

    public string? LatestUrl { get; set; }

    public string? EmergencyUrl { get; set; }

    public ExitCodeCheck? ExitCode { get; set; }
}

public sealed class UpdateResponse
{
    [Required]
    public UpdateConfig Instance { get; set; } = new();

    public SharedConfig? Shared { get; set; }

    [Required]
    public List<UpdateRelease> Releases { get; set; } = new();
}