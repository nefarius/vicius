using System.Text.Json;
using System.Text.Json.Serialization;

using Nefarius.Vicius.Abstractions.Converters;

namespace Nefarius.Vicius.Abstractions.Tests;

/// <summary>
///     Serializer options that match the example server's FastEndpoints configuration.
/// </summary>
internal static class ManifestJson
{
    public static JsonSerializerOptions Options { get; } = Create();

    private static JsonSerializerOptions Create()
    {
        JsonSerializerOptions options = new()
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            PropertyNameCaseInsensitive = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
            PreferredObjectCreationHandling = JsonObjectCreationHandling.Populate
        };

        options.Converters.Add(new DateTimeOffsetConverter());
        options.Converters.Add(new JsonStringEnumConverter());
        return options;
    }
}
