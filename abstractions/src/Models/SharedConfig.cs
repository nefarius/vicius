using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     Parameters that might be provided by both the server and the local configuration.
/// </summary>
/// <remarks>Keep in sync with <see cref="MergedConfig" />.</remarks>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
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
                null => null,
                RegistryValueConfig _ => ProductVersionDetectionMethod.RegistryValue,
                FileVersionConfig _ => ProductVersionDetectionMethod.FileVersion,
                FileSizeConfig _ => ProductVersionDetectionMethod.FileSize,
                FileChecksumConfig _ => ProductVersionDetectionMethod.FileChecksum,
                CustomExpressionConfig _ => ProductVersionDetectionMethod.CustomExpression,
                FixedVersionConfig _ => ProductVersionDetectionMethod.FixedVersion,
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
    /// <remarks>By default, a temporary directory of the current user is used.</remarks>
    public DownloadLocationConfig? DownloadLocation { get; set; }

    /// <summary>
    ///     Gets or sets whether the updater should run as a temporary copy instead from the origin directory.
    /// </summary>
    /// <example>
    ///     This feature is useful when the updater is shipped with a product using Windows Installer, which detects open
    ///     processes that block an upgrade. Enabling this setting re-launches the updater process from a temporary copy so
    ///     on-the-fly upgrades in the origin directory become possible.
    /// </example>
    public bool? RunAsTemporaryCopy { get; set; }

    /// <summary>
    ///     Global Authenticode verification mode for downloaded setups.
    /// </summary>
    public SignatureVerificationMode? SignatureVerificationMode { get; set; }

    /// <summary>
    ///     Global Authenticode comparison policy.
    /// </summary>
    public SignatureComparisonPolicy? SignaturePolicy { get; set; }

    /// <summary>
    ///     Global Authenticode pin strategy.
    /// </summary>
    public SignatureVerificationStrategy? SignatureStrategy { get; set; }

    /// <summary>
    ///     Explicit certificate pin (used with <see cref="SignatureVerificationStrategy.FromConfiguration" />).
    /// </summary>
    public SignatureConfig? SignatureConfig { get; set; }

    /// <summary>
    ///     When true, the "Remind me tomorrow" button is hidden in the update UI.
    /// </summary>
    public bool? HideRemindButton { get; set; }

    /// <summary>
    ///     Base64-encoded Windows .ico data used as the window and taskbar icon at runtime.
    /// </summary>
    /// <remarks>
    ///     The server should encode a valid .ico file (which may contain a PNG-compressed entry
    ///     for Vista+ compatibility) as standard Base64 and supply it here.  The client decodes
    ///     it in memory and applies it via WM_SETICON; the compiled-in icon is kept as fallback
    ///     if the field is absent or the data cannot be decoded.
    /// </remarks>
    public string? IconBase64 { get; set; }
}
