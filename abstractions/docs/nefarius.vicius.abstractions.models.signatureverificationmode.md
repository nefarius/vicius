# SignatureVerificationMode

Namespace: Nefarius.Vicius.Abstractions.Models

Controls whether Authenticode signature verification of the setup binary is enforced.

```csharp
public enum SignatureVerificationMode
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [SignatureVerificationMode](./nefarius.vicius.abstractions.models.signatureverificationmode.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| Disabled | 0 | No Authenticode check is performed. |
| WhenPresent | 1 | Check only if the setup is signed; unsigned setups are allowed. This is the default. |
| Required | 2 | A valid Authenticode chain is mandatory; unsigned setups are rejected. |
