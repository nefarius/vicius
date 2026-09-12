using System.Globalization;
using System.Text.Json;

using Nefarius.Vicius.Abstractions.Converters;

namespace Nefarius.Vicius.Abstractions.Tests;

public sealed class DateTimeOffsetConverterTests
{
    private static readonly DateTimeOffsetConverter Converter = new();

    [Fact]
    public void Write_emits_utc_iso8601_without_fractional_seconds()
    {
        DateTimeOffset local = new(2024, 6, 1, 14, 30, 45, 123, TimeSpan.FromHours(2));

        string json = JsonSerializer.Serialize(local, new JsonSerializerOptions { Converters = { Converter } });

        Assert.Equal("\"2024-06-01T12:30:45Z\"", json);
    }

    [Fact]
    public void Read_parses_exact_utc_format()
    {
        DateTimeOffset parsed = JsonSerializer.Deserialize<DateTimeOffset>(
            "\"2024-06-01T12:30:45Z\"",
            new JsonSerializerOptions { Converters = { Converter } });

        Assert.Equal(DateTimeOffset.Parse("2024-06-01T12:30:45Z", CultureInfo.InvariantCulture), parsed);
    }

    [Fact]
    public void Read_rejects_values_outside_the_exact_format()
    {
        Assert.Throws<FormatException>(() =>
            JsonSerializer.Deserialize<DateTimeOffset>(
                "\"2024-06-01T12:30:45.123Z\"",
                new JsonSerializerOptions { Converters = { Converter } }));
    }
}
