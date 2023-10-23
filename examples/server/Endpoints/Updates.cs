using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;

using NJsonSchema;
using NJsonSchema.Generation;

namespace Nefarius.Vicius.Example.Server.Endpoints;

public sealed class Updates : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/schema/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        JsonSchemaGeneratorSettings settings = new JsonSchemaGeneratorSettings()
        {
            ActualSerializerSettings =
            {
                Formatting = Formatting.Indented,
                ContractResolver = new CamelCasePropertyNamesContractResolver()
            }
        };
        JsonSchemaGenerator generator = new JsonSchemaGenerator(settings);
        JsonSchema? schema = generator.Generate(typeof(UpdateResponse));
        string? schemaData = schema.ToJson();

        await SendStringAsync(schemaData, contentType: "application/json", cancellation: ct);
    }
}