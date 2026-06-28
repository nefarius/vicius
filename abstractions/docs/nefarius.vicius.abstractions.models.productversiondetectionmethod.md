# ProductVersionDetectionMethod

Namespace: Nefarius.Vicius.Abstractions.Models

The detection method of the installed software to use on the client.

```csharp
public enum ProductVersionDetectionMethod
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [ProductVersionDetectionMethod](./nefarius.vicius.abstractions.models.productversiondetectionmethod.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| RegistryValue | 0 | Performs checking a specified registry hive, key and value. |
| FileVersion | 1 | Performs checking a specified file version resource. |
| FileSize | 2 | Performs checking a specified file size for matching. |
| FileChecksum | 3 | Calculates and compares the hash of a given file. |
| CustomExpression | 4 | Custom inja expression. |
| FixedVersion | 5 | A fixed version string. |
