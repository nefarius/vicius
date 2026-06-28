# DateTimeOffsetConverter

Namespace: Nefarius.Vicius.Abstractions.Converters

[DateTimeOffset](https://learn.microsoft.com/dotnet/api/system.datetimeoffset) to ISO 8601 (or custom formats) string (UTC) converter.

```csharp
public sealed class DateTimeOffsetConverter : System.Text.Json.Serialization.JsonConverter<System.DateTimeOffset>
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [JsonConverter](https://learn.microsoft.com/dotnet/api/system.text.json.serialization.jsonconverter) → [JsonConverter](https://learn.microsoft.com/dotnet/api/system.text.json.serialization.jsonconverter-1)<[DateTimeOffset](https://learn.microsoft.com/dotnet/api/system.datetimeoffset)> → [DateTimeOffsetConverter](./nefarius.vicius.abstractions.converters.datetimeoffsetconverter.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-handlenull"/>**HandleNull**

```csharp
public bool HandleNull { get; }
```

#### Property Value

[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)<br>

### <a id="properties-type"/>**Type**

```csharp
public Type Type { get; }
```

#### Property Value

[Type](https://learn.microsoft.com/dotnet/api/system.type)<br>

## Constructors

### <a id="constructors-.ctor"/>**DateTimeOffsetConverter(String)**

```csharp
public DateTimeOffsetConverter(string format)
```

#### Parameters

`format` [String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Methods

### <a id="methods-read"/>**Read(ref Utf8JsonReader, Type, JsonSerializerOptions)**

```csharp
public DateTimeOffset Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
```

#### Parameters

`reader` [Utf8JsonReader&](https://learn.microsoft.com/dotnet/api/system.text.json.utf8jsonreader&)<br>

`typeToConvert` [Type](https://learn.microsoft.com/dotnet/api/system.type)<br>

`options` [JsonSerializerOptions](https://learn.microsoft.com/dotnet/api/system.text.json.jsonserializeroptions)<br>

#### Returns

[DateTimeOffset](https://learn.microsoft.com/dotnet/api/system.datetimeoffset)

### <a id="methods-write"/>**Write(Utf8JsonWriter, DateTimeOffset, JsonSerializerOptions)**

```csharp
public void Write(Utf8JsonWriter writer, DateTimeOffset date, JsonSerializerOptions options)
```

#### Parameters

`writer` [Utf8JsonWriter](https://learn.microsoft.com/dotnet/api/system.text.json.utf8jsonwriter)<br>

`date` [DateTimeOffset](https://learn.microsoft.com/dotnet/api/system.datetimeoffset)<br>

`options` [JsonSerializerOptions](https://learn.microsoft.com/dotnet/api/system.text.json.jsonserializeroptions)<br>
