# ExitCodeCheck

Namespace: Nefarius.Vicius.Abstractions.Models

Setup exit code parameters.

```csharp
public sealed class ExitCodeCheck
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ExitCodeCheck](./nefarius.vicius.abstractions.models.exitcodecheck.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-skipcheck"/>**SkipCheck**

Ignore whatever exit code we got if true.

```csharp
public bool SkipCheck { get; set; }
```

#### Property Value

[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)<br>

### <a id="properties-successcodes"/>**SuccessCodes**

One or more exit codes that signify success.

```csharp
public List<Int32> SuccessCodes { get; }
```

#### Property Value

[List](https://learn.microsoft.com/dotnet/api/system.collections.generic.list-1)<[Int32](https://learn.microsoft.com/dotnet/api/system.int32)><br>

## Constructors

### <a id="constructors-.ctor"/>**ExitCodeCheck()**

```csharp
public ExitCodeCheck()
```
