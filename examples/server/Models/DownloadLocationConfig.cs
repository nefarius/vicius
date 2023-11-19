using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     Inja template details about the download location path.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
public sealed class DownloadLocationConfig
{
    /// <summary>
    ///     The absolute path or inja template.
    /// </summary>
    [Required]
    public required string Input { get; set; }
    
    /// <summary>
    ///     Optional inja template data.
    /// </summary>
    public Dictionary<string, string>? Data { get; set; }
}