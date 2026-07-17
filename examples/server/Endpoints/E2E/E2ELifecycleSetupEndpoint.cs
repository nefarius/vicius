using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario for async-shutdown lifecycle testing: a normal, fast download/verify
///     followed by a setup.exe that is deliberately kept "running" for a while (via the
///     E2E_EXIT_DELAY_MS environment variable inherited by the child process; see
///     examples/e2e-setup-stub/Program.cs). This gives the harness a wide, deterministic
///     window to close the updater window while InstanceConfig::setupTask is genuinely in
///     flight, so it can verify closing is bounded by the cooperative stop_source
///     (see main.cpp's WM_QUIT handling and InstanceConfig::WaitForSetupToFinish) rather
///     than by the full setup process duration.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ELifecycleSetupEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/LifecycleSetup/updates.json");
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

        string exePath = Path.Combine(artifactsDir, "setup.exe");
        if (!File.Exists(exePath))
            ThrowError("E2E fixture 'setup.exe' not found in artifacts directory", 500);

        string checksum = E2EGuard.ComputeSha256(exePath);
        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E LifecycleSetup Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Lifecycle Setup Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/setup.exe",
                    Checksum = new ChecksumParameters
                    {
                        ChecksumAlg = ChecksumAlgorithm.SHA256,
                        Checksum = checksum
                    },
                    ExitCode = new ExitCodeCheck
                    {
                        SuccessCodes = { 0 }
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
