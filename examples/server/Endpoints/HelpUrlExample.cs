using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Shared;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demoing showing the help button and help URL.
/// </summary>
internal sealed class HelpUrlExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/HelpUrl/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                HelpUrl = "https://docs.nefarius.at/projects/Vicius/Examples/Help-Page/"
            },
            Shared = new SharedConfig
            {
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKLM,
                    Key = @"SOFTWARE\Nefarius Software Solutions e.U.\HidHide",
                    Value = "Version"
                }
            },
            Releases = { Examples.MinimalDemoRelease }
        };

        await Send.OkAsync(response, ct);
    }
}