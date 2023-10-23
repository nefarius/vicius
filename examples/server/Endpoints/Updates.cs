using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

using NJsonSchema;

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
        JsonSchema? schema = JsonSchema.FromType<UpdateResponse>();
        string? schemaData = schema.ToJson();

        await SendStringAsync(schemaData, contentType: "application/json", cancellation: ct);
    }
}