# UpdateResponse

Namespace: Nefarius.Vicius.Abstractions.Models

An instance returned by the remote update API.

```csharp
public sealed class UpdateResponse
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [UpdateResponse](./nefarius.vicius.abstractions.models.updateresponse.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-instance"/>**Instance**

Update instance configuration. Parameters applying to the entire product/tenant.

```csharp
public UpdateConfig Instance { get; set; }
```

#### Property Value

[UpdateConfig](./nefarius.vicius.abstractions.models.updateconfig.md)<br>

### <a id="properties-releases"/>**Releases**

The available update releases.

```csharp
public List<UpdateRelease> Releases { get; set; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[UpdateRelease](./nefarius.vicius.abstractions.models.updaterelease.md)><br>

### <a id="properties-shared"/>**Shared**

Parameters that might be provided by both the server and the local configuration.

```csharp
public SharedConfig Shared { get; set; }
```

#### Property Value

[SharedConfig](./nefarius.vicius.abstractions.models.sharedconfig.md)<br>

## Constructors

### <a id="constructors-.ctor"/>**UpdateResponse()**

```csharp
public UpdateResponse()
```
