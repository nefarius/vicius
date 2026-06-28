# ChecksumParameters

Namespace: Nefarius.Vicius.Abstractions.Models

Parameters for checksum/hash calculation.

```csharp
public sealed class ChecksumParameters
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ChecksumParameters](./nefarius.vicius.abstractions.models.checksumparameters.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### <a id="properties-checksum"/>**Checksum**

The checksum/hash value to compare against.

```csharp
public string Checksum { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-checksumalg"/>**ChecksumAlg**

The algorithm to use to calculate the checksum/hash.

```csharp
public ChecksumAlgorithm ChecksumAlg { get; set; }
```

#### Property Value

[ChecksumAlgorithm](./nefarius.vicius.abstractions.models.checksumalgorithm.md)<br>

## Constructors

### <a id="constructors-.ctor"/>**ChecksumParameters()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public ChecksumParameters()
```
