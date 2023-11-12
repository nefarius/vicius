﻿using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection.Templates;

/// <summary>
///     Demos sophisticated product detection using the template engine.
/// </summary>
internal sealed class ProductByDisplayNameEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/ProductByDisplayName/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                Detection = new CustomExpressionConfig()
                {
                    Input = @"{% set result=productByDisplayName(parameters.product) %}{{ log(result.count) }}",
                    Data = new Dictionary<string, string>
                    {
                        { "product", @"DaVinci" }
                    }
                }
            },
            Releases =
            {
                new UpdateRelease()
                {
                    Name = "Demo",
                    Summary = "Demo",
                    Version = System.Version.Parse("2.0.0"),
                    DownloadUrl = "https://example.com",
                    PublishedAt = DateTimeOffset.Now,
                    DetectionSize = 4096
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}