# Authority

Namespace: Nefarius.Vicius.Abstractions.Models

Determines which configuration source wins when both the local file and the remote server provide the same field.

```csharp
public enum Authority
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [Authority](./nefarius.vicius.abstractions.models.authority.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| Local | 0 | The local configuration file takes priority over the server response. |
| Remote | 1 | The server-side instructions take priority over the local configuration file. This is the default. |
