using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Example.Server.Models;

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

    /// <summary>
    ///     URL pointing to a help article opening when an update error occurred.
    /// </summary>
    [Required]
    public required string InstallationErrorUrl { get; set; }

    /// <summary>
    ///     The preferred setup download directory.
    /// </summary>
    [Required]
    public required DownloadLocationConfig DownloadLocation { get; set; }
}