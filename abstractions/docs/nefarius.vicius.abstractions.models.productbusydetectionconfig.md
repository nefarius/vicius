# ProductBusyDetectionConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Configuration for detecting whether the watched product is currently running,
 used to defer the update notification dialog until the product has been closed.

```csharp
public sealed class ProductBusyDetectionConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ProductBusyDetectionConfig](./nefarius.vicius.abstractions.models.productbusydetectionconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

## Properties

### <a id="properties-executablepaths"/>**ExecutablePaths**

Optional absolute paths (or inja templates) for exact full-path matching against
 running processes. Rendered via the inja template engine before comparison.

```csharp
public List<String> ExecutablePaths { get; set; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[String](https://learn.microsoft.com/dotnet/api/system.string)><br>

### <a id="properties-imagenames"/>**ImageNames**

Process image base names matched case-insensitively against running processes.

```csharp
public List<String> ImageNames { get; set; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[String](https://learn.microsoft.com/dotnet/api/system.string)><br>

### <a id="properties-maxwaitminutes"/>**MaxWaitMinutes**

Maximum minutes to wait before giving up and deferring to the next scheduled run.
 Default: 180 (3 hours). The agent hard-clamps this value to 180 regardless of what
 is configured here.

```csharp
public Nullable<Int32> MaxWaitMinutes { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Int32](https://learn.microsoft.com/dotnet/api/system.int32)><br>

### <a id="properties-pollintervalseconds"/>**PollIntervalSeconds**

Seconds between successive in-use re-checks. Default: 60.

```csharp
public Nullable<Int32> PollIntervalSeconds { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Int32](https://learn.microsoft.com/dotnet/api/system.int32)><br>

## Constructors

### <a id="constructors-.ctor"/>**ProductBusyDetectionConfig()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public ProductBusyDetectionConfig()
```
