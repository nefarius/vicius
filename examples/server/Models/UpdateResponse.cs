using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     An instance returned by the remote update API.
/// </summary>
[SuppressMessage("ReSharper", "CollectionNeverQueried.Global")]
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
public sealed class UpdateResponse
{
    /// <summary>
    ///     Update instance configuration. Parameters applying to the entire product/tenant.
    /// </summary>
    public UpdateConfig? Instance { get; set; }

    /// <summary>
    ///     Parameters that might be provided by both the server and the local configuration.
    /// </summary>
    public SharedConfig? Shared { get; set; }

    /// <summary>
    ///     The available update releases.
    /// </summary>
    [Required]
    public List<UpdateRelease> Releases { get; } = new();
}