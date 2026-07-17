using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: the download URL responds with an HTTP redirect (302) to another
///     location instead of the artifact itself. <c>CURLOPT_REDIR_PROTOCOLS_STR</c> is
///     restricted to <c>https</c> only (see <c>web::RestrictRedirectProtocols</c>), so
///     libcurl must refuse to follow this plain-HTTP redirect target and the download
///     must fail rather than silently retrieving the artifact through a redirect hop.
///     This is the closest local proof of the https-&gt;http downgrade protection
///     achievable without a TLS-terminating test server: the mechanism under test
///     (the allow-listed redirect protocol) does not depend on the scheme of the
///     original request, only on the scheme of the redirect target.
///     Expected updater exit code: <c>NV_E_DOWNLOAD_FAILED = 107</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ERedirectDowngradeEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/RedirectDowngrade/updates.json");
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
                WindowTitle = "E2E RedirectDowngrade Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Redirect Downgrade Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/RedirectDowngrade/redirect",
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
///     Responds with a 302 redirect to the real artifact. Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ERedirectDowngradeRedirectEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/RedirectDowngrade/redirect");
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
        await Send.RedirectAsync($"{baseUrl}/api/e2e/artifacts/payload.zip", allowRemoteRedirects: true);
    }
}
