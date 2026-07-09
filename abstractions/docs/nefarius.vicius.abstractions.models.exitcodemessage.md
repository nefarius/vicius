# ExitCodeMessage

Namespace: Nefarius.Vicius.Abstractions.Models

Per-exit-code UI message entry used in [ExitCodeCheck.Messages](./nefarius.vicius.abstractions.models.exitcodecheck.md#messages).

```csharp
public sealed class ExitCodeMessage
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ExitCodeMessage](./nefarius.vicius.abstractions.models.exitcodemessage.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-buttontext"/>**ButtonText**

Optional custom label for the help/more-info button.
 Defaults to `"Open help page"` when omitted.

```csharp
public string ButtonText { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-helpurl"/>**HelpUrl**

Optional URL opened by the help/more-info button shown alongside the message.

```csharp
public string HelpUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-issuccess"/>**IsSuccess**

When `true`, this exit code is treated as a success condition even if it is
 absent from [ExitCodeCheck.SuccessCodes](./nefarius.vicius.abstractions.models.exitcodecheck.md#successcodes).

```csharp
public Nullable<Boolean> IsSuccess { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)><br>

### <a id="properties-message"/>**Message**

The message displayed in the updater UI when this exit code is encountered.

```csharp
public string Message { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**ExitCodeMessage()**

```csharp
public ExitCodeMessage()
```
