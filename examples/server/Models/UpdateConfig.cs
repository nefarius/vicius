using System.Diagnostics.CodeAnalysis;

using NJsonSchema.Annotations;

namespace Nefarius.Vicius.Example.Server.Models;

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
    ///     The URL to the latest updater binary. Redirects are supported.
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

    /// <summary>
    ///     URL pointing to a help article opening when the user was presented with an error dialog.
    /// </summary>
    public string? ErrorFallbackUrl { get; set; }
}