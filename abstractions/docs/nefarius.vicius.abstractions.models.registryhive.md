# RegistryHive

Namespace: Nefarius.Vicius.Abstractions.Models

The registry hive to search for keys/values under.

```csharp
public enum RegistryHive
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [RegistryHive](./nefarius.vicius.abstractions.models.registryhive.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

## Fields

| Name | Value | Description |
| --- | --: | --- |
| HKCU | 0 | HKEY_CURRENT_USER hive. |
| HKLM | 1 | HKEY_LOCAL_MACHINE hive. |
| HKCR | 2 | HKEY_CLASSES_ROOT hive. |
