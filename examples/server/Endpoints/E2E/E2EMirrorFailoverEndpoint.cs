using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: manifest contains a broken primary <c>downloadUrl</c> (responds 404) and a
///     working mirror URL pointing at the real <c>payload.zip</c> artifact.
///     The updater must exhaust the primary download, switch to the mirror, and complete successfully.
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EMirrorFailoverEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MirrorFailover/updates.json");
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

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E MirrorFailover Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Mirror Failover Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    // Deliberately points at a non-existent artifact; the updater will receive 404.
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/does-not-exist.zip",
                    // The mirror URL points at the real artifact and is tried after the primary fails.
                    MirrorUrls = [$"{baseUrl}/api/e2e/artifacts/payload.zip"],
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
