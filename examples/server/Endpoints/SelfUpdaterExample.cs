using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demos providing an update to the updater itself.
/// </summary>
internal sealed class SelfUpdaterExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/SelfUpdater/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance =
                new UpdateConfig
                {
                    LatestVersion = System.Version.Parse("1.0.0"),
                    /*
                     just an example URL; if you patch the updater with a different type of
                     executable, obviously you would brick the remote installation ;)
                     */
                    LatestUrl = "https://downloads.nefarius.at/other/nefarius/vpatch/vpatch.exe"
                }
        };

        await SendOkAsync(response, ct);
    }
}