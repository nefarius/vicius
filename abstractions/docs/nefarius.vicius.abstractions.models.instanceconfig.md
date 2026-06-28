# InstanceConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Local configuration file model (the fields written to the embedded JSON resource of each updater binary).

```csharp
public sealed class InstanceConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [InstanceConfig](./nefarius.vicius.abstractions.models.instanceconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

**Remarks:**

Only the four fields present in the C++ `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT` macro are
 included here. Runtime-only members of `InstanceConfig` are not serialized and are therefore excluded.

## Properties

### <a id="properties-authority"/>**Authority**

Which configuration source wins when both local and remote provide the same field.

```csharp
public Authority Authority { get; set; }
```

#### Property Value

[Authority](./nefarius.vicius.abstractions.models.authority.md)<br>

### <a id="properties-fallbackserverurltemplates"/>**FallbackServerUrlTemplates**

Optional pre-rendered fallback URLs tried in order when [InstanceConfig.ServerUrlTemplate](./nefarius.vicius.abstractions.models.instanceconfig.md#serverurltemplate) fails.

```csharp
public List<String> FallbackServerUrlTemplates { get; set; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[String](https://learn.microsoft.com/dotnet/api/system.string)><br>

### <a id="properties-filenameregex"/>**FilenameRegex**

Regular expression used to match the setup filename inside a downloaded archive.

```csharp
public string FilenameRegex { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-serverurltemplate"/>**ServerUrlTemplate**

The URL template used to build the update manifest request (may contain inja placeholders).

```csharp
public string ServerUrlTemplate { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**InstanceConfig()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public InstanceConfig()
```
