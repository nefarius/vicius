using System.Text.Json;
using System.Text.Json.Serialization;

namespace Nefarius.Vicius.Example.Server.Converters;

internal sealed class CustomDateTimeOffsetConverter : JsonConverter<DateTimeOffset>
{
    private readonly string _format;

    public CustomDateTimeOffsetConverter(string format)
    {
        _format = format;
    }

    public override void Write(Utf8JsonWriter writer, DateTimeOffset date, JsonSerializerOptions options)
    {
        writer.WriteStringValue(date.ToString(_format));
    }

    public override DateTimeOffset Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
    {
        return DateTime.ParseExact(reader.GetString(), _format, null);
    }
}