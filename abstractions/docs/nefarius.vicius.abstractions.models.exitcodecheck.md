# ExitCodeCheck

Namespace: Nefarius.Vicius.Abstractions.Models

Setup exit code parameters.

```csharp
public sealed class ExitCodeCheck
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ExitCodeCheck](./nefarius.vicius.abstractions.models.exitcodecheck.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-messages"/>**Messages**

Optional per-exit-code UI messages, keyed by decimal exit code string (e.g. `"3010"`).
 An entry with [ExitCodeMessage.IsSuccess](./nefarius.vicius.abstractions.models.exitcodemessage.md#issuccess) set to `true` also promotes that
 code to a success condition without requiring it to appear in [ExitCodeCheck.SuccessCodes](./nefarius.vicius.abstractions.models.exitcodecheck.md#successcodes).

```csharp
public Dictionary<String, ExitCodeMessage> Messages { get; set; }
```

#### Property Value

[Dictionary](https://learn.microsoft.com/dotnet/api/system.collections.generic.dictionary-2)<[String](https://learn.microsoft.com/dotnet/api/system.string), [ExitCodeMessage](./nefarius.vicius.abstractions.models.exitcodemessage.md)><br>

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
