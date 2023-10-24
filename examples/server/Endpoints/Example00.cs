using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

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
        var response = new UpdateResponse()
        {
            Instance =
            {
                EmergencyUrl = "https://docs.nefarius.at/projects/Vicius/Emergency-Feature/"
            }
        };
        
        await SendOkAsync(response, ct);
    }
}
