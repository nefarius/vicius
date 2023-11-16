using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;
using Nefarius.Vicius.Example.Server.Shared;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection;

/// <summary>
///     Demos custom expression method using the template engine.
/// </summary>
internal sealed class CustomExpressionEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/CustomExpression/updates.json");
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
                    Input = @"{% if remote.releases.0.name == ""Demo"" %}true{% else %}false{% endif %}",
                    Data = new Dictionary<string, string>
                    {
                        { "value", "2" }
                    }
                }
            },
            Releases = { Examples.MinimalDemoRelease }
        };

        await SendOkAsync(response, ct);
    }
}