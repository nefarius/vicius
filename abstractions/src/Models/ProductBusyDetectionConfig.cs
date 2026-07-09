using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     Configuration for detecting whether the watched product is currently running,
///     used to defer the update notification dialog until the product has been closed.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class ProductBusyDetectionConfig
{
    /// <summary>
    ///     Process image base names matched case-insensitively against running processes.
    /// </summary>
    /// <example>["HidHide.exe", "HidHideMon.exe"]</example>
    [Required]
    public required List<string> ImageNames { get; set; }

    /// <summary>
    ///     Optional absolute paths (or inja templates) for exact full-path matching against
    ///     running processes. Rendered via the inja template engine before comparison.
    /// </summary>
    public List<string>? ExecutablePaths { get; set; }

    /// <summary>
    ///     Seconds between successive in-use re-checks. Default: 60.
    /// </summary>
    public int? PollIntervalSeconds { get; set; }

    /// <summary>
    ///     Maximum minutes to wait before giving up and deferring to the next scheduled run.
    ///     Default: 180 (3 hours). The agent hard-clamps this value to 180 regardless of what
    ///     is configured here.
    /// </summary>
    public int? MaxWaitMinutes { get; set; }
}
