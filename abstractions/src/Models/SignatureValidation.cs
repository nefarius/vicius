using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;

using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     The policy to abide by when comparing signatures.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum SignatureComparisonPolicy
{
    /// <summary>
    ///     A valid, trusted Authenticode chain is sufficient. This is the default.
    /// </summary>
    [EnumMember(Value = nameof(Relaxed))]
    Relaxed,

    /// <summary>
    ///     Fails with an error when the remote setup signature doesn't match the expected value.
    /// </summary>
    /// <remarks>
    ///     If <see cref="SignatureVerificationStrategy.FromUpdaterBinary" /> is configured and the updater binary is not
    ///     signed properly, applying updates will always fail with an error.
    /// </remarks>
    [EnumMember(Value = nameof(Strict))]
    Strict
}

/// <summary>
///     The strategy on how to verify if the setup signature is trusted.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum SignatureVerificationStrategy
{
    /// <summary>
    ///     Extract the signature information from the updater binary and use that info to validate the signed setup.
    /// </summary>
    /// <remarks>
    ///     If <see cref="SignatureComparisonPolicy.Strict" /> is configured and the updater binary is not signed properly,
    ///     applying updates will always fail with an error.
    /// </remarks>
    [EnumMember(Value = nameof(FromUpdaterBinary))]
    FromUpdaterBinary,

    /// <summary>
    ///     Use the information provided in the <see cref="SignatureConfig" /> to validate the signed setup.
    /// </summary>
    [EnumMember(Value = nameof(FromConfiguration))]
    FromConfiguration
}

/// <summary>
///     Controls whether Authenticode signature verification of the setup binary is enforced.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum SignatureVerificationMode
{
    /// <summary>
    ///     No Authenticode check is performed.
    /// </summary>
    [EnumMember(Value = nameof(Disabled))]
    Disabled,

    /// <summary>
    ///     Check only if the setup is signed; unsigned setups are allowed. This is the default.
    /// </summary>
    [EnumMember(Value = nameof(WhenPresent))]
    WhenPresent,

    /// <summary>
    ///     A valid Authenticode chain is mandatory; unsigned setups are rejected.
    /// </summary>
    [EnumMember(Value = nameof(Required))]
    Required
}

/// <summary>
///     Authenticode certificate pin details.
/// </summary>
/// <remarks>
///     Cert fields by renewal stability:
///     <list type="bullet">
///         <item>Stable: <see cref="SubjectName" /> — tied to the legal entity; default pin for FromUpdaterBinary.</item>
///         <item>
///             Volatile: <see cref="SerialNumber" />, <see cref="ThumbprintSha1" />, <see cref="ThumbprintSha256" /> —
///             change on every renewal; only safe when travelling inside the signed manifest (FromConfiguration).
///         </item>
///         <item>Semi-stable: <see cref="IssuerName" /> — changes when CA rotates intermediates or you switch CA.</item>
///     </list>
/// </remarks>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
public sealed class SignatureConfig
{
    /// <summary>
    ///     The issuer name (CA display name). Semi-stable — use with caution across renewals.
    /// </summary>
    public string? IssuerName { get; set; }

    /// <summary>
    ///     The subject / organization name. Stable across renewals; default FromUpdaterBinary pin field.
    /// </summary>
    public string? SubjectName { get; set; }

    /// <summary>
    ///     The certificate serial number as a hex string. Volatile — only safe via signed manifest (FromConfiguration).
    /// </summary>
    public string? SerialNumber { get; set; }

    /// <summary>
    ///     The SHA-1 thumbprint (hex). Volatile — only safe via signed manifest (FromConfiguration).
    /// </summary>
    public string? ThumbprintSha1 { get; set; }

    /// <summary>
    ///     The SHA-256 thumbprint (hex). Volatile — only safe via signed manifest (FromConfiguration).
    /// </summary>
    public string? ThumbprintSha256 { get; set; }
}
