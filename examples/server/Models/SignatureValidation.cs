using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;

using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Example.Server.Models;

/// <summary>
///     The policy to abide by when comparing signatures.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum SignatureComparisonPolicy
{
    /// <summary>
    ///     Allows the user to make the decision to continue when the remote setup signature doesn't match the expected value.
    ///     This is the default.
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
    ///     Use the information provided in the <see cref="SharedConfig" /> to validate the signed setup.
    /// </summary>
    [EnumMember(Value = nameof(FromConfiguration))]
    FromConfiguration
}

/// <summary>
///     Authenticode signature details.
/// </summary>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "MemberCanBePrivate.Global")]
public sealed class SignatureConfig
{
    /// <summary>
    ///     The issuer name (CA display name).
    /// </summary>
    public string? IssuerName { get; set; }

    /// <summary>
    ///     The subject name (e.g. friendly signer company name).
    /// </summary>
    public string? SubjectName { get; set; }

    /// <summary>
    ///     The signing certificate serial number as a hex-string. The format depends on the CA used.
    /// </summary>
    public string? SerialNumber { get; set; }
}