using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;

using NJsonSchema;
using NJsonSchema.Generation;
using NJsonSchema.NewtonsoftJson.Generation;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Delivers the JSON schema of all types involved in the update response.
/// </summary>
internal sealed class SchemaEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/vicius/master/schema.json");
        AllowAnonymous();
        Options(x => x.WithTags("Schemas"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        NewtonsoftJsonSchemaGeneratorSettings settings = new()
        {
            SerializerSettings = new JsonSerializerSettings
            {
                Formatting = Formatting.Indented,
                // transform PascalCase properties to camelCase fields
                ContractResolver = new CamelCasePropertyNamesContractResolver()
            }
        };
        JsonSchemaGenerator generator = new(settings);
        JsonSchema? schema = generator.Generate(typeof(UpdateResponse));
        string? schemaData = schema.ToJson();

        await SendStringAsync(schemaData, contentType: "application/json", cancellation: ct);
    }
}