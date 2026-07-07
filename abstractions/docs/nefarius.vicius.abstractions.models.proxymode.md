# ProxyMode

Namespace: Nefarius.Vicius.Abstractions.Models

Controls how the updater obtains an HTTP proxy for outbound connections.

```csharp
public enum ProxyMode
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [ProxyMode](./nefarius.vicius.abstractions.models.proxymode.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| System | 0 | Auto-detect from the Windows/WinINET system proxy settings (static proxy, PAC URL, or WPAD). This is the default; the updater will work behind corporate firewalls without any configuration. |
| None | 1 | Always connect directly — no proxy, even if one is configured in the OS. |
| Manual | 2 | Use the explicit URL specified in [NetworkConfig.ProxyUrl](./nefarius.vicius.abstractions.models.networkconfig.md#proxyurl). |
