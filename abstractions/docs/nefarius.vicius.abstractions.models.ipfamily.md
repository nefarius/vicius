# IpFamily

Namespace: Nefarius.Vicius.Abstractions.Models

Controls which IP address family libcurl uses for name resolution.

```csharp
public enum IpFamily
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [IpFamily](./nefarius.vicius.abstractions.models.ipfamily.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| Any | 0 | Let libcurl choose (default). |
| V4 | 1 | Force IPv4 name resolution. Useful when IPv6 connectivity is unreliable. |
| V6 | 2 | Force IPv6 name resolution. |
