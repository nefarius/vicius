using System.Text.Json;
using System.Text.Json.Serialization;

using Nefarius.Vicius.Abstractions.Converters;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Serializer options that match the FastEndpoints pipeline:
///     camelCase, omit nulls, ISO 8601 UTC timestamps, enum names.
///     Use these for both serving a manifest and computing its minisign sidecar
///     so the signed bytes equal the served bytes.
/// </summary>
internal static class ManifestJson
{
    internal static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters =
        {
            new DateTimeOffsetConverter(),
            new JsonStringEnumConverter()
        }
    };
}
