using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demoing only the emergency feature, see https://docs.nefarius.at/projects/Vicius/Emergency-Feature/
/// </summary>
internal sealed class Example00Endpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/Example00/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                EmergencyUrl = "https://docs.nefarius.at/projects/Vicius/Examples/Landing-Page/"
            }
        };

        await SendOkAsync(response, ct);
    }
}