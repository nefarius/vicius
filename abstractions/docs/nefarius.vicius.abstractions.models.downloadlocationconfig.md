# DownloadLocationConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Inja template details about the download location path.

```csharp
public sealed class DownloadLocationConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [DownloadLocationConfig](./nefarius.vicius.abstractions.models.downloadlocationconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### <a id="properties-data"/>**Data**

Optional inja template data.

```csharp
public Dictionary<String, String> Data { get; set; }
```

#### Property Value

[Dictionary](https://learn.microsoft.com/dotnet/api/system.collections.generic.dictionary-2)<[String](https://learn.microsoft.com/dotnet/api/system.string), [String](https://learn.microsoft.com/dotnet/api/system.string)><br>

### <a id="properties-input"/>**Input**

The absolute path or inja template.

```csharp
public string Input { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**DownloadLocationConfig()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public DownloadLocationConfig()
```
