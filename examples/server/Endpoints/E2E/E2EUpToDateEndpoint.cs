using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: product is already up-to-date (server offers version 2.0.0,
///     driver passes <c>--force-local-version 2.0.0</c>).
///     Expected updater exit code: <c>NV_S_UP_TO_DATE = 202</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EUpToDateEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/UpToDate/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E UpToDate Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Release v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    // Never fetched — client exits as up-to-date before downloading
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/payload.zip"
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
