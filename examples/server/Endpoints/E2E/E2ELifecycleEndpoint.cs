using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario for async-shutdown lifecycle testing: both the changelog image
///     referenced from the release summary and the release artifact itself are served
///     through an artificial delay, so the harness has a wide, deterministic window to
///     close the updater window while the image-download task and/or the release-download
///     task are genuinely still in flight (see markdown::Shutdown and
///     InstanceConfig::WaitForDownloadToFinish).
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ELifecycleEndpoint : EndpointWithoutRequest
{
    // Comfortably longer than the harness waits before sending WM_CLOSE, so the tasks are
    // still running at close time, and comfortably shorter than the harness's bounded exit
    // timeout would be if the wait were naively for the full delay (it must not be, after
    // the fix: the process should exit long before this elapses).
    private const int DelayMs = 8000;

    public override void Configure()
    {
        Get("api/e2e/Lifecycle/updates.json");
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

        string artifactsDir = E2EGuard.ArtifactsDir;
        if (string.IsNullOrEmpty(artifactsDir))
            ThrowError("E2E_ARTIFACTS_DIR is not set", 500);

        string zipPath = Path.Combine(artifactsDir, "payload.zip");
        if (!File.Exists(zipPath))
            ThrowError("E2E fixture 'payload.zip' not found in artifacts directory", 500);

        string checksum = E2EGuard.ComputeSha256(zipPath);
        string baseUrl = E2EGuard.BaseUrl(HttpContext);
        // Reuses payload.zip bytes as a stand-in "image": the client's WIC decode will fail
        // once the delayed response eventually arrives, but that happens long after the
        // harness has already closed the window, so decode success is irrelevant here.
        string delayedArtifactUrl = $"{baseUrl}/api/e2e/artifacts/payload.zip?delayMs={DelayMs}";

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E Lifecycle Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Lifecycle Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = $"![delayed]({delayedArtifactUrl})",
                    DownloadUrl = delayedArtifactUrl,
                    Checksum = new ChecksumParameters
                    {
                        ChecksumAlg = ChecksumAlgorithm.SHA256,
                        Checksum = checksum
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
