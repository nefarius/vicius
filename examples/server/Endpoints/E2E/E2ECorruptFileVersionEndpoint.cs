using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: detection via the FILEVERSION resource of a non-PE binary.
///     <c>GetWin32ResourceFileVersion</c> returns the sentinel <c>0.0.1</c> for files with no
///     readable version resource, so the server release <c>2.0.0</c> is seen as newer and the
///     update proceeds — exercising the file-version detection path with an unreadable target.
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ECorruptFileVersionEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/CorruptFileVersion/updates.json");
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
                WindowTitle = "E2E CorruptFileVersion Updater",
                // The harness creates corrupt_version.bin (random non-PE bytes) in
                // E2E_ARTIFACTS_DIR. The inja envar() callback resolves E2E_ARTIFACTS_DIR
                // at runtime so the path is portable across CI runs.
                Detection = new FileVersionConfig
                {
                    // envar(dir) resolves the E2E_ARTIFACTS_DIR environment variable at
                    // runtime inside the updater process (inherited from the test harness).
                    Input = @"{{ envar(dir) }}\corrupt_version.bin",
                    Data = new Dictionary<string, string>
                    {
                        { "dir", "E2E_ARTIFACTS_DIR" }
                    }
                    // Statement defaults to FILEVERSION; omitted for consistency with examples.
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E ZIP Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/payload.zip",
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
