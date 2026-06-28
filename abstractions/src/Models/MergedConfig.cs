using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     The shared configuration that has been merged with local and remote parameters.
/// </summary>
/// <remarks>Keep in sync with <see cref="SharedConfig" />.</remarks>
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
    /// <remarks>By default, a temporary directory of the current user is used.</remarks>
    [Required]
    public required DownloadLocationConfig DownloadLocation { get; set; }

    /// <summary>
    ///     Gets or sets whether the updater should run as a temporary copy instead from the origin directory.
    /// </summary>
    [Required]
    public required bool RunAsTemporaryCopy { get; set; }

    /// <summary>
    ///     Authenticode verification mode for downloaded setups. Default: <see cref="SignatureVerificationMode.WhenPresent" />.
    /// </summary>
    [Required]
    public SignatureVerificationMode SignatureVerificationMode { get; set; } = SignatureVerificationMode.WhenPresent;

    /// <summary>
    ///     Authenticode comparison policy. Default: <see cref="SignatureComparisonPolicy.Relaxed" />.
    /// </summary>
    [Required]
    public SignatureComparisonPolicy SignaturePolicy { get; set; } = SignatureComparisonPolicy.Relaxed;

    /// <summary>
    ///     Authenticode pin strategy. Default: <see cref="SignatureVerificationStrategy.FromUpdaterBinary" />.
    /// </summary>
    [Required]
    public SignatureVerificationStrategy SignatureStrategy { get; set; } = SignatureVerificationStrategy.FromUpdaterBinary;

    /// <summary>
    ///     Explicit certificate pin (used with <see cref="SignatureVerificationStrategy.FromConfiguration" />).
    /// </summary>
    public SignatureConfig? SignatureConfig { get; set; }

    /// <summary>
    ///     When true, the "Remind me tomorrow" button is hidden in the update UI. Default: false.
    /// </summary>
    [Required]
    public bool HideRemindButton { get; set; } = false;
}
