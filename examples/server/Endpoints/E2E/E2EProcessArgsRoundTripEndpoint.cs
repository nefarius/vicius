using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available, EXE installer (setup stub), whose manifest-supplied
///     <c>launchArguments</c> raw command-line fragment contains a quoted argument with an
///     embedded space, a quoted argument with an embedded escaped double-quote, and a quoted
///     argument ending in an escaped trailing backslash.
///     Regression coverage for src/InstanceConfig.Setup.cpp's process launch: proves the
///     writable command-line buffer and explicit application name changes still deliver
///     these tokens to the child process unchanged (see tests/e2e/run-e2e.ps1's
///     ProcessArgsRoundTrip scenario, which reads them back via E2E_ARGS_LOG_FILE).
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EProcessArgsRoundTripEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/ProcessArgsRoundTrip/updates.json");
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

        // Built via explicit concatenation (rather than an escaped/verbatim literal) so the
        // exact intended bytes are unambiguous. Quote (Q) and backslash (B) are single
        // characters; the resulting raw Windows command-line fragment is:
        //     "arg with spaces" "quote\"inside" "trailingback\\"
        // which CommandLineToArgvW-style parsing must split into exactly three argv tokens:
        //     arg with spaces | quote"inside | trailingback\
        const string q = "\"";
        const string b = "\\";
        string launchArguments = q + "arg with spaces" + q + " " +
                                  q + "quote" + b + q + "inside" + q + " " +
                                  q + "trailingback" + b + b + q;

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Test Product",
                WindowTitle = "E2E ProcessArgsRoundTrip Updater"
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Process Args Round Trip Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = DateTimeOffset.UtcNow,
                    Summary = string.Empty,
                    DownloadUrl = $"{baseUrl}/api/e2e/artifacts/setup.exe",
                    Checksum = new ChecksumParameters
                    {
                        ChecksumAlg = ChecksumAlgorithm.SHA256,
                        Checksum = checksum
                    },
                    LaunchArguments = launchArguments,
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
