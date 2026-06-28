using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available but the manifest intentionally contains a wrong checksum.
///     The updater downloads the real artifact then detects the hash mismatch and aborts.
///     Expected updater exit code: <c>NV_E_SIGNATURE_INVALID = 116</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EChecksumMismatchEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/ChecksumMismatch/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await SendNotFoundAsync(ct);
            return;
        }

        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E ChecksumMismatch Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Checksum Mismatch Update v2.0.0",
                    Version = Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/payload.zip",
                    // Deliberately wrong checksum — all zeros
                    Checksum = new ChecksumParameters
                    {
                        ChecksumAlg = ChecksumAlgorithm.SHA256,
                        Checksum = new string('0', 64)
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
