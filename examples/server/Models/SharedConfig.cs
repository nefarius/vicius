using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     Parameters that might be provided by both the server and the local configuration.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
public sealed class SharedConfig
{
    private ProductVersionDetectionImplementation? _detection;

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
    public ProductVersionDetectionMethod? DetectionMethod { get; private set; }

    /// <summary>
    ///     The details of the selected <see cref="DetectionMethod" />.
    /// </summary>
    public ProductVersionDetectionImplementation? Detection
    {
        get => _detection;
        set
        {
            DetectionMethod = value switch
            {
                RegistryValueConfig _ => ProductVersionDetectionMethod.RegistryValue,
                FileVersionConfig _ => ProductVersionDetectionMethod.FileVersion,
                FileSizeConfig _ => ProductVersionDetectionMethod.FileSize,
                FileChecksumConfig _ => ProductVersionDetectionMethod.FileChecksum,
                CustomExpressionConfig _ => ProductVersionDetectionMethod.CustomExpression,
                _ => DetectionMethod
            };

            _detection = value;
        }
    }

    /// <summary>
    ///     URL pointing to a help article opening when an update error occurred.
    /// </summary>
    public string? InstallationErrorUrl { get; set; }

    /// <summary>
    ///     The preferred setup download directory.
    /// </summary>
    public DownloadLocationConfig? DownloadLocation { get; set; }
}