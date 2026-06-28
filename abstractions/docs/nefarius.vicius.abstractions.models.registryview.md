# RegistryView

Namespace: Nefarius.Vicius.Abstractions.Models

The alternate registry view to apply.

```csharp
public enum RegistryView
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [ValueType](https://learn.microsoft.com/dotnet/api/system.valuetype) → [Enum](https://learn.microsoft.com/dotnet/api/system.enum) → [RegistryView](./nefarius.vicius.abstractions.models.registryview.md)<br>
Implements [IComparable](https://learn.microsoft.com/dotnet/api/system.icomparable), [ISpanFormattable](https://learn.microsoft.com/dotnet/api/system.ispanformattable), [IFormattable](https://learn.microsoft.com/dotnet/api/system.iformattable), [IConvertible](https://learn.microsoft.com/dotnet/api/system.iconvertible)<br>
Attributes JsonConverterAttribute

**Remarks:**

https://learn.microsoft.com/en-us/windows/win32/winprog64/accessing-an-alternate-registry-view

## Fields

| Name | Value | Description |
| --- | --: | --- |
| Default | 0 | Default behavior (no flags set). |
| WOW64_64KEY | 1 | Access a 64-bit key from either a 32-bit or 64-bit application. |
| WOW64_32KEY | 2 | Access a 32-bit key from either a 32-bit or 64-bit application. |
