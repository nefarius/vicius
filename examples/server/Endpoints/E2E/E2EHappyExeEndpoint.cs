using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available, EXE installer (setup stub), correct SHA-256 checksum,
///     exit code 0 mapped to success.
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EHappyExeEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/HappyExe/updates.json");
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

        string exePath = Path.Combine(E2EGuard.ArtifactsDir, "setup.exe");
        string checksum = E2EGuard.ComputeSha256(exePath);
        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E HappyExe Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E EXE Update v2.0.0",
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
