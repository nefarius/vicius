using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario (negative control): detection via a registry key that does not exist.
///     A missing registry key is a structural failure (not a corrupt-version value), so the
///     updater must hard-fail with NV_E_PRODUCT_DETECTION rather than treating it as outdated.
///     Expected updater exit code: <c>NV_E_PRODUCT_DETECTION = 105</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EMissingRegistryEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MissingRegistry/updates.json");
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
                WindowTitle = "E2E MissingRegistry Updater",
                // This key is deliberately never created by the harness; the updater must
                // return NV_E_PRODUCT_DETECTION (105) rather than falling back to outdated.
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKCU,
                    Key = @"Software\Nefarius\ViciusE2E\DoesNotExist",
                    Value = "Version"
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
