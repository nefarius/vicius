using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available, ZIP payload served with an attacker-controlled
///     <c>Content-Disposition</c> header that attempts a path-traversal filename
///     (<c>../../evil.exe</c>, quoted). The updater must reject the filename, keep its
///     own temp file name, and still complete the update successfully.
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EMaliciousContentDispositionEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MaliciousContentDisposition/updates.json");
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
                WindowTitle = "E2E MaliciousContentDisposition Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Malicious Content-Disposition Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/MaliciousContentDisposition/artifact",
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

/// <summary>
///     Serves <c>payload.zip</c> with a malicious, quoted, path-traversal
///     <c>Content-Disposition</c> filename. Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EMaliciousContentDispositionArtifactEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MaliciousContentDisposition/artifact");
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
        string zipPath = Path.Combine(artifactsDir, "payload.zip");

        if (!File.Exists(zipPath))
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        // Deliberately hostile: quoted, path-traversal, wrong extension. The updater's
        // sanitizer must reject this outright rather than trying to interpret it.
        HttpContext.Response.Headers["Content-Disposition"] = "attachment; filename=\"..\\..\\evil.exe\"";
        HttpContext.Response.ContentType = "application/zip";
        HttpContext.Response.StatusCode = 200;
        await using FileStream fs = File.OpenRead(zipPath);
        await fs.CopyToAsync(HttpContext.Response.Body, ct);
    }
}
