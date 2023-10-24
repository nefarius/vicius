using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints;

internal sealed class Example00Endpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/Example00/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        await SendOkAsync(ct);
    }
}
