﻿using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection.Templates;

/// <summary>
///     Demos sophisticated product detection using the template engine.
/// </summary>
internal sealed class IniTemplateEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/INI/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                Detection = new FileSizeConfig
                {
                    Input = @"{{ inival(file, section, key) }}",
                    Data = new Dictionary<string, string>
                    {
                        { "file", @"E:\GOG Galaxy Games\Worms Armageddon\goglog.ini" },
                        { "section", "1462173886" },
                        { "key", "Dir_0" }
                    }
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}