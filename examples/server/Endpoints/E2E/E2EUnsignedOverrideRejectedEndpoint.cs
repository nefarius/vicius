using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: update available, ZIP payload, correct SHA-256 checksum, but the manifest
///     is served <b>unsigned</b> (no <c>.minisig</c> sidecar / requires no compiled-in
///     <c>NV_MANIFEST_PUBLIC_KEY</c>) while attempting to weaken all four Authenticode
///     verification-policy fields (<c>SignatureVerificationMode</c>, <c>SignaturePolicy</c>,
///     <c>SignatureStrategy</c>, <c>SignatureConfig</c>).
///     <para>
///         Regression coverage for the verification-policy trust boundary in
///         <c>InstanceConfig::RequestUpdateInfo</c> (src/InstanceConfig.Web.cpp): an
///         unverifiable manifest must never be able to change these fields. The update must
///         still succeed (the local default <c>WhenPresent</c> mode already accepts the
///         unsigned test payload), but the log must show every override being rejected — see
///         the <c>UnsignedOverrideRejected</c> scenario in tests/e2e/run-e2e.ps1, which asserts
///         on the "Ignoring remote ... override" warning lines.
///     </para>
///     Expected updater exit code: <c>NV_S_UPDATE_FINISHED = 203</c>.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EUnsignedOverrideRejectedEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/UnsignedOverrideRejected/updates.json");
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
                WindowTitle = "E2E UnsignedOverrideRejected Updater",
                // Attempted weakening: this manifest is served unsigned (this endpoint's
                // route has no NV_MANIFEST_PUBLIC_KEY / .minisig involved at all), so the
                // client must ignore all four of these and keep its local/default policy.
                SignatureVerificationMode = SignatureVerificationMode.Disabled,
                SignaturePolicy = SignatureComparisonPolicy.Relaxed,
                SignatureStrategy = SignatureVerificationStrategy.FromConfiguration,
                SignatureConfig = new SignatureConfig
                {
                    SubjectName = "Attacker-Controlled CN"
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Unsigned Override Rejected Update v2.0.0",
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
