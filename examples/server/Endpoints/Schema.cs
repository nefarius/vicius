using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;

using NJsonSchema;
using NJsonSchema.Generation;

namespace Nefarius.Vicius.Example.Server.Endpoints;

internal sealed class SchemaEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/example/schema/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        JsonSchemaGeneratorSettings settings = new()
        {
            SerializerSettings = new JsonSerializerSettings
            {
                Formatting = Formatting.Indented,
                ContractResolver = new CamelCasePropertyNamesContractResolver()
            }
        };
        JsonSchemaGenerator generator = new(settings);
        JsonSchema? schema = generator.Generate(typeof(UpdateResponse));
        string? schemaData = schema.ToJson();

        await SendStringAsync(schemaData, contentType: "application/json", cancellation: ct);
    }
}