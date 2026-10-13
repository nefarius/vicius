using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     Manifests for the MSI launch scenarios. The artifact route serves the bytes with no
///     Content-Disposition, so the updater has to name and classify the payload itself.
/// </summary>
internal static class E2EMsiPayloadManifest
{
    public static UpdateResponse Create(
        HttpContext http,
        string artifactFileName,
        string windowTitle,
        string releaseName,
        bool expectSuccessfulInstall)
    {
        string artifactsDir = E2EGuard.ArtifactsDir;
        if (string.IsNullOrEmpty(artifactsDir))
            throw new InvalidOperationException("E2E_ARTIFACTS_DIR is not set");

        string artifactPath = Path.Combine(artifactsDir, artifactFileName);
        if (!File.Exists(artifactPath))
            throw new FileNotFoundException($"E2E fixture '{artifactFileName}' not found", artifactPath);

        string baseUrl = E2EGuard.BaseUrl(http);
        UpdateRelease release = new()
        {
            Name = releaseName,
            Version = System.Version.Parse("2.0.0"),
            PublishedAt = DateTimeOffset.UtcNow,
            Summary = string.Empty,
            DownloadUrl = $"{baseUrl}/api/e2e/artifacts/{artifactFileName}",
            Checksum = new ChecksumParameters
            {
                ChecksumAlg = ChecksumAlgorithm.SHA256,
                Checksum = E2EGuard.ComputeSha256(artifactPath)
            }
        };

        if (expectSuccessfulInstall)
        {
            release.LaunchArguments = "/qn /norestart";
            release.ExitCode = new ExitCodeCheck
            {
                SuccessCodes = { 0 }
            };
        }

        return new UpdateResponse
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = windowTitle
            },
            Releases = { release }
        };
    }
}

/// <summary>
///     HTML error page served under a .msi name. The updater must reject it before launch.
///     Expected updater exit code: <c>NV_E_SETUP_FAILED = 109</c>.
/// </summary>
internal sealed class E2EMsiHtmlRejectedEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MsiHtmlRejected/updates.json");
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

        await Send.OkAsync(E2EMsiPayloadManifest.Create(
            HttpContext, "error-page.msi", "E2E MSI HTML Rejected",
            "E2E HTML payload v2.0.0", expectSuccessfulInstall: false), ct);
    }
}

/// <summary>
///     OLE compound-file stub that is not a Windows Installer database, served as .msi.
///     Expected updater exit code: <c>NV_E_SETUP_FAILED = 109</c>.
/// </summary>
internal sealed class E2EMsiStubRejectedEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MsiStubRejected/updates.json");
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

        await Send.OkAsync(E2EMsiPayloadManifest.Create(
            HttpContext, "compound-stub.msi", "E2E MSI Stub Rejected",
            "E2E compound stub v2.0.0", expectSuccessfulInstall: false), ct);
    }
}

/// <summary>
///     Minimal valid per-user MSI. The updater must launch it with msiexec and record exit 0.
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
/// </summary>
internal sealed class E2EMsiPayloadEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/MsiPayload/updates.json");
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

        await Send.OkAsync(E2EMsiPayloadManifest.Create(
            HttpContext, "payload.msi", "E2E MSI Payload",
            "E2E MSI Update v2.0.0", expectSuccessfulInstall: true), ct);
    }
}
