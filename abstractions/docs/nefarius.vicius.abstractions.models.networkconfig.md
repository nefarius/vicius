# NetworkConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Per-deployment network resilience configuration read from the sidecar JSON under
 `instance.network`. All fields are optional; sensible defaults apply when absent.

```csharp
public sealed class NetworkConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [NetworkConfig](./nefarius.vicius.abstractions.models.networkconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-dohurl"/>**DohUrl**

DNS-over-HTTPS resolver URL. When non-empty, libcurl uses DoH for all name resolutions,
 bypassing the system resolver. Useful against DNS censorship or poisoning.
 Example: `https://cloudflare-dns.com/dns-query`

```csharp
public string DohUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-ipfamily"/>**IpFamily**

IP address family preference for DNS resolution. Defaults to [IpFamily.Any](./nefarius.vicius.abstractions.models.ipfamily.md#any).
 Set to [IpFamily.V4](./nefarius.vicius.abstractions.models.ipfamily.md#v4) or [IpFamily.V6](./nefarius.vicius.abstractions.models.ipfamily.md#v6) to force a specific family.

```csharp
public IpFamily IpFamily { get; set; }
```

#### Property Value

[IpFamily](./nefarius.vicius.abstractions.models.ipfamily.md)<br>

### <a id="properties-pinnedhosts"/>**PinnedHosts**

Static host→IP pin entries. For each entry the updater bypasses DNS and connects directly
 to the pinned IP address, while still using the hostname in TLS SNI.

```csharp
public List<PinnedHost> PinnedHosts { get; set; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[PinnedHost](./nefarius.vicius.abstractions.models.pinnedhost.md)><br>

### <a id="properties-proxymode"/>**ProxyMode**

How the updater should obtain an HTTP proxy. Defaults to [ProxyMode.System](./nefarius.vicius.abstractions.models.proxymode.md#system)
 (auto-detect from Windows/WinINET settings).

```csharp
public ProxyMode ProxyMode { get; set; }
```

#### Property Value

[ProxyMode](./nefarius.vicius.abstractions.models.proxymode.md)<br>

### <a id="properties-proxyurl"/>**ProxyUrl**

Explicit proxy URL used when [NetworkConfig.ProxyMode](./nefarius.vicius.abstractions.models.networkconfig.md#proxymode) is [ProxyMode.Manual](./nefarius.vicius.abstractions.models.proxymode.md#manual).
 Format: `http[s]://[user:pass@]host:port`

```csharp
public string ProxyUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**NetworkConfig()**

```csharp
public NetworkConfig()
```
