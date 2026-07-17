using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available, ZIP payload exercising archive-extraction edge cases
///     that previously crashed or threw an uncaught exception out of the setup task:
///     <list type="bullet">
///         <item>an explicit empty directory entry ("emptydir/") in the archive,</item>
///         <item>a <see cref="ZipExtractFileDisposition.DeleteIfPresent" /> override for a file that
///             does not exist at the destination (first install), and</item>
///         <item>a normal file extracted with the default <see cref="ZipExtractFileDisposition.CreateOrReplace" />
///             disposition.</item>
///     </list>
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EZipEdgeCasesEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/ZipEdgeCases/updates.json");
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

        string zipPath = Path.Combine(artifactsDir, "payload_edge.zip");
        if (!File.Exists(zipPath))
            ThrowError("E2E fixture 'payload_edge.zip' not found in artifacts directory", 500);

        string checksum = E2EGuard.ComputeSha256(zipPath);
        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E ZipEdgeCases Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E ZIP Edge Cases Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/payload_edge.zip",
                    Checksum = new ChecksumParameters
                    {
                        ChecksumAlg = ChecksumAlgorithm.SHA256,
                        Checksum = checksum
                    },
                    ZipExtractDefaultFileDisposition = ZipExtractFileDisposition.CreateOrReplace,
                    // "missing-file.txt" is never present in a fresh install directory: this override
                    // exercises the DeleteIfPresent-absent-at-destination path that used to throw.
                    ZipExtractFileDispositionOverrides = new Dictionary<string, ZipExtractFileDisposition>
                    {
                        ["missing-file.txt"] = ZipExtractFileDisposition.DeleteIfPresent
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
