# ZipExtractFileDisposition

Namespace: Nefarius.Vicius.Abstractions.Models

How to treat files when installing a .zip file.

```csharp
public enum ZipExtractFileDisposition
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [ZipExtractFileDisposition](./nefarius.vicius.abstractions.models.zipextractfiledisposition.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| CreateIfAbsent | 0 | Create the extracted file if it doesn't already exist locally. |
| CreateOrReplace | 1 | Create or replace a locally existing file with the extracted one of the same name. |
| DeleteIfPresent | 2 | Delete the local file, if found. |
