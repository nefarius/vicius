# SignatureComparisonPolicy

Namespace: Nefarius.Vicius.Abstractions.Models

The policy to abide by when comparing signatures.

```csharp
public enum SignatureComparisonPolicy
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [SignatureComparisonPolicy](./nefarius.vicius.abstractions.models.signaturecomparisonpolicy.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| Relaxed | 0 | A valid, trusted Authenticode chain is sufficient. This is the default. |
| Strict | 1 | Fails with an error when the remote setup signature doesn't match the expected value. |
