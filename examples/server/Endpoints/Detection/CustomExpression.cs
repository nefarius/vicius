using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection;

/// <summary>
///     Demos sophisticated product detection using the template engine.
/// </summary>
internal sealed class CustomExpressionEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/CustomExpression/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                Detection = new CustomExpressionConfig()
                {
                    //Input = @"{% if int(parameters.value) == 2 %}true{% else %}false{% endif %}",
                    Input = @"{% if remote.releases.0.name == ""Demo"" %}true{% else %}false{% endif %}",
                    Data = new Dictionary<string, string>
                    {
                        { "value", "2" }
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