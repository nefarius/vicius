using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demoing only the emergency feature, see https://docs.nefarius.at/projects/Vicius/Emergency-Feature/
/// </summary>
internal sealed class EmergencyUrlExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/EmergencyUrl/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
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