# VersionResource

Namespace: Nefarius.Vicius.Abstractions.Models

Describes which version resource fixed-value to read.

```csharp
public enum VersionResource
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [VersionResource](./nefarius.vicius.abstractions.models.versionresource.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| FILEVERSION | 0 | Binary version number for the file. The version consists of two 32-bit integers, defined by four 16-bit integers. For example, "FILEVERSION 3,10,0,61" is translated into two doublewords: 0x0003000a and 0x0000003d, in that order. Therefore, if version is defined by the DWORD values dw1 and dw2, they need to appear in the FILEVERSION statement as follows: HIWORD(dw1), LOWORD(dw1), HIWORD(dw2), LOWORD(dw2). |
| PRODUCTVERSION | 1 | Binary version number for the product with which the file is distributed. The version parameter is two 32-bit integers, defined by four 16-bit integers. For more information about version, see the FILEVERSION description. |
