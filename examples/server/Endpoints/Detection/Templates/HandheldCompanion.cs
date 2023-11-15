using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection.Templates;

/// <summary>
///     Demos sophisticated product detection using the template engine.
/// </summary>
internal sealed class HandheldCompanionEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/HandheldCompanion/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "Handheld Companion",
                Detection = new CustomExpressionConfig
                {
                    Input =
                        // query installed product
                        "{% set result=productBy(parameters.value, parameters.product) %}" +
                        // at least one match found
                        "{% if int(result.count) > 0 %}" +
                        // compare local version to remote release version
                        "{% if versionLt(result.results.0.displayVersion, remote.releases.0.version) %}" +
                        // return isOutdated = true
                        "true" +
                        "{% else %}" +
                        // return isOutdated = false
                        "false" +
                        "{% endif %}" +
                        // no matches found
                        "{% else %}" +
                        // return isOutdated = true
                        "true" +
                        "{% endif %}",
                    Data = new Dictionary<string, string>
                    {
                        // registry value to query
                        { "value", "DisplayName" },
                        // regex to search for
                        { "product", "Handheld Companion" }
                    }
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Demo",
                    Summary = "Demo",
                    Version = System.Version.Parse("0.18.0.6"),
                    DownloadUrl = "https://example.com",
                    PublishedAt = DateTimeOffset.Now
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}