﻿using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

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