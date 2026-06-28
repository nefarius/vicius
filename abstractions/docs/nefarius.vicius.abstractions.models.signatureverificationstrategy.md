# SignatureVerificationStrategy

Namespace: Nefarius.Vicius.Abstractions.Models

The strategy on how to verify if the setup signature is trusted.

```csharp
public enum SignatureVerificationStrategy
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [SignatureVerificationStrategy](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| FromUpdaterBinary | 0 | Extract the signature information from the updater binary and use that info to validate the signed setup. |
| FromConfiguration | 1 | Use the information provided in the [SignatureConfig](./nefarius.vicius.abstractions.models.signatureconfig.md) to validate the signed setup. |
