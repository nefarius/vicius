# RegistryValueConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Queries a specific registry value and matches it against the selected [UpdateRelease](./nefarius.vicius.abstractions.models.updaterelease.md) version.

```csharp
public sealed class RegistryValueConfig : ProductVersionDetectionImplementation
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ProductVersionDetectionImplementation](./nefarius.vicius.abstractions.models.productversiondetectionimplementation.md) → [RegistryValueConfig](./nefarius.vicius.abstractions.models.registryvalueconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute), [KnownTypeAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.serialization.knowntypeattribute)

## Properties

### <a id="properties-hive"/>**Hive**

The hive.

```csharp
public RegistryHive Hive { get; set; }
```

#### Property Value

[RegistryHive](./nefarius.vicius.abstractions.models.registryhive.md)<br>

### <a id="properties-key"/>**Key**

The (sub-)key.

```csharp
public string Key { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-value"/>**Value**

The value name.

```csharp
public string Value { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-view"/>**View**

The alternate view.

```csharp
public RegistryView View { get; set; }
```

#### Property Value

[RegistryView](./nefarius.vicius.abstractions.models.registryview.md)<br>

## Constructors

### <a id="constructors-.ctor"/>**RegistryValueConfig()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public RegistryValueConfig()
```
