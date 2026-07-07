# PinnedHost

Namespace: Nefarius.Vicius.Abstractions.Models

A static host→IP pin that bypasses DNS for a specific hostname.

```csharp
public sealed class PinnedHost
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [PinnedHost](./nefarius.vicius.abstractions.models.pinnedhost.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

**Remarks:**

The SNI hostname in TLS is still taken from the URL, so the server's TLS certificate must
 match the hostname. Useful when DNS is blocked or poisoned but the IP address is reachable.

## Properties

### <a id="properties-address"/>**Address**

IP address to use instead of DNS, e.g. `104.21.0.1`. May be IPv4 or IPv6.

```csharp
public string Address { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-host"/>**Host**

Hostname to pin, e.g. `vicius.api.nefarius.systems`.

```csharp
public string Host { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-port"/>**Port**

Port number to pin. Defaults to `443`.

```csharp
public int Port { get; set; }
```

#### Property Value

[Int32](https://learn.microsoft.com/dotnet/api/system.int32)<br>

## Constructors

### <a id="constructors-.ctor"/>**PinnedHost()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public PinnedHost()
```
