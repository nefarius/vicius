using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demoing only the emergency feature, see https://docs.nefarius.at/projects/Vicius/Emergency-Feature/
/// </summary>
internal sealed class HelpUrlExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/HelpUrl/updates.json");
        AllowAnonymous();
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
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Demo Update",
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-3),
                    Version = System.Version.Parse("2.0.0"),
                    Summary = "",
                    DownloadUrl = ""
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}