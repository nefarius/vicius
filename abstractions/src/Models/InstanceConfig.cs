using System.ComponentModel.DataAnnotations;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;

using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     Determines which configuration source wins when both the local file and the remote server provide the same field.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum Authority
{
    /// <summary>
    ///     The local configuration file takes priority over the server response.
    /// </summary>
    [EnumMember(Value = nameof(Local))]
    Local,

    /// <summary>
    ///     The server-side instructions take priority over the local configuration file. This is the default.
    /// </summary>
    [EnumMember(Value = nameof(Remote))]
    Remote
}

/// <summary>
///     Local configuration file model (the fields written to the embedded JSON resource of each updater binary).
/// </summary>
/// <remarks>
///     Only the four fields present in the C++ <c>NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT</c> macro are
///     included here. Runtime-only members of <c>InstanceConfig</c> are not serialized and are therefore excluded.
/// </remarks>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class InstanceConfig
{
    /// <summary>
    ///     The URL template used to build the update manifest request (may contain inja placeholders).
    /// </summary>
    [Required]
    public required string ServerUrlTemplate { get; set; }

    /// <summary>
    ///     Optional pre-rendered fallback URLs tried in order when <see cref="ServerUrlTemplate" /> fails.
    /// </summary>
    public List<string>? FallbackServerUrlTemplates { get; set; }

    /// <summary>
    ///     Regular expression used to match the setup filename inside a downloaded archive.
    /// </summary>
    [Required]
    public required string FilenameRegex { get; set; }

    /// <summary>
    ///     Which configuration source wins when both local and remote provide the same field.
    /// </summary>
    [Required]
    public Authority Authority { get; set; } = Authority.Remote;
}
