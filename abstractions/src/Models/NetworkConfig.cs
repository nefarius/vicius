using System.Diagnostics.CodeAnalysis;
using System.Runtime.Serialization;

using Newtonsoft.Json;
using Newtonsoft.Json.Converters;

namespace Nefarius.Vicius.Abstractions.Models;

/// <summary>
///     Controls how the updater obtains an HTTP proxy for outbound connections.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum ProxyMode
{
    /// <summary>
    ///     Auto-detect from the Windows/WinINET system proxy settings (static proxy, PAC URL, or WPAD).
    ///     This is the default; the updater will work behind corporate firewalls without any configuration.
    /// </summary>
    [EnumMember(Value = nameof(System))]
    System,

    /// <summary>
    ///     Always connect directly — no proxy, even if one is configured in the OS.
    /// </summary>
    [EnumMember(Value = nameof(None))]
    None,

    /// <summary>
    ///     Use the explicit URL specified in <see cref="NetworkConfig.ProxyUrl" />.
    /// </summary>
    [EnumMember(Value = nameof(Manual))]
    Manual
}

/// <summary>
///     Controls which IP address family libcurl uses for name resolution.
/// </summary>
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[JsonConverter(typeof(StringEnumConverter))]
public enum IpFamily
{
    /// <summary>
    ///     Let libcurl choose (default).
    /// </summary>
    [EnumMember(Value = nameof(Any))]
    Any,

    /// <summary>
    ///     Force IPv4 name resolution. Useful when IPv6 connectivity is unreliable.
    /// </summary>
    [EnumMember(Value = nameof(V4))]
    V4,

    /// <summary>
    ///     Force IPv6 name resolution.
    /// </summary>
    [EnumMember(Value = nameof(V6))]
    V6
}

/// <summary>
///     A static host→IP pin that bypasses DNS for a specific hostname.
/// </summary>
/// <remarks>
///     The SNI hostname in TLS is still taken from the URL, so the server's TLS certificate must
///     match the hostname. Useful when DNS is blocked or poisoned but the IP address is reachable.
/// </remarks>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class PinnedHost
{
    /// <summary>
    ///     Hostname to pin, e.g. <c>vicius.api.nefarius.systems</c>.
    /// </summary>
    public required string Host { get; set; }

    /// <summary>
    ///     Port number to pin. Defaults to <c>443</c>.
    /// </summary>
    public int Port { get; set; } = 443;

    /// <summary>
    ///     IP address to use instead of DNS, e.g. <c>104.21.0.1</c>. May be IPv4 or IPv6.
    /// </summary>
    public required string Address { get; set; }
}

/// <summary>
///     Per-deployment network resilience configuration read from the sidecar JSON under
///     <c>instance.network</c>. All fields are optional; sensible defaults apply when absent.
/// </summary>
/// <example>
/// <code lang="json">
/// {
///   "instance": {
///     "network": {
///       "proxyMode": "System",
///       "proxyUrl": "http://user:pass@proxy.corp:8080",
///       "dohUrl": "https://cloudflare-dns.com/dns-query",
///       "ipFamily": "Any",
///       "pinnedHosts": [
///         { "host": "vicius.api.nefarius.systems", "port": 443, "address": "104.21.0.1" }
///       ]
///     }
///   }
/// }
/// </code>
/// </example>
[SuppressMessage("ReSharper", "UnusedAutoPropertyAccessor.Global")]
[SuppressMessage("ReSharper", "UnusedMember.Global")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
public sealed class NetworkConfig
{
    /// <summary>
    ///     How the updater should obtain an HTTP proxy. Defaults to <see cref="ProxyMode.System" />
    ///     (auto-detect from Windows/WinINET settings).
    /// </summary>
    public ProxyMode ProxyMode { get; set; } = ProxyMode.System;

    /// <summary>
    ///     Explicit proxy URL used when <see cref="ProxyMode" /> is <see cref="ProxyMode.Manual" />.
    ///     Format: <c>http[s]://[user:pass@]host:port</c>
    /// </summary>
    public string? ProxyUrl { get; set; }

    /// <summary>
    ///     DNS-over-HTTPS resolver URL. When non-empty, libcurl uses DoH for all name resolutions,
    ///     bypassing the system resolver. Useful against DNS censorship or poisoning.
    ///     Example: <c>https://cloudflare-dns.com/dns-query</c>
    /// </summary>
    public string? DohUrl { get; set; }

    /// <summary>
    ///     IP address family preference for DNS resolution. Defaults to <see cref="IpFamily.Any" />.
    ///     Set to <see cref="IpFamily.V4" /> or <see cref="IpFamily.V6" /> to force a specific family.
    /// </summary>
    public IpFamily IpFamily { get; set; } = IpFamily.Any;

    /// <summary>
    ///     Static host→IP pin entries. For each entry the updater bypasses DNS and connects directly
    ///     to the pinned IP address, while still using the hostname in TLS SNI.
    /// </summary>
    public List<PinnedHost>? PinnedHosts { get; set; }
}
