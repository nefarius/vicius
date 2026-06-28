using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: manifest advertises a far-future updater version (999.0.0.0) so the
///     self-update check in the main process triggers unconditionally. The self-updater
///     DLL then downloads the binary, fails Authenticode (unsigned), and restores the
///     original. The parent process returns <c>NV_S_SELF_UPDATER = 201</c> before
///     the DLL does any of that work.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ESelfUpdateEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/SelfUpdate/updates.json");
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
            Instance = new UpdateConfig
            {
                // A version that will always exceed whatever the built updater's version is.
                LatestVersion = System.Version.Parse("999.0.0.0"),
                LatestUrl = $"{baseUrl}/api/e2e/artifacts/updater_selfupdate.exe"
            },
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E SelfUpdate Updater"
            },
            // Dummy release so the manifest parses cleanly; it is never reached
            // because the self-update check short-circuits before product detection.
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Release v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/payload.zip"
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
