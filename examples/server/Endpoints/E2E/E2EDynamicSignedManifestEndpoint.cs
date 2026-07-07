using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

using FastEndpoints;

using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Services;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: manifest built and signed at request time by the server using minisign-net.
///     Two products are served under this endpoint:
///     <list type="bullet">
///         <item>
///             <c>DynamicSignedManifest</c> — canonical manifest + valid signature; updater should
///             succeed (exit 203).
///         </item>
///         <item>
///             <c>DynamicTamperedManifest</c> — tampered manifest body served together with a
///             signature that covers the <em>untampered</em> body; updater should reject the manifest
///             (exit 104).
///         </item>
///     </list>
///     Routes handled (manufacturer <c>e2eSigDyn</c>):
///     <list type="bullet">
///         <item><c>GET api/e2eSigDyn/{product}/updates.json</c></item>
///         <item><c>GET api/e2eSigDyn/{product}/updates.json.minisig</c></item>
///     </list>
///     Only active when <c>VICIUS_E2E=1</c> and the signer is configured.
/// </summary>
internal sealed class E2EDynamicSignedManifestEndpoint : Endpoint<E2EDynamicSignedManifestRequest>
{
    // Serializer options that produce the same bytes the FastEndpoints pipeline uses globally:
    //   - camelCase property names
    //   - null values omitted
    //   - DateTimeOffset as ISO 8601 UTC string
    //   - enums as their string names
    // These options must be used for both serving the manifest and computing the signature so that
    // the bytes that are signed == the bytes that are served.
    internal static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters =
        {
            new DateTimeOffsetConverter(),
            new JsonStringEnumConverter()
        }
    };

    private readonly MinisignManifestSigner _signer;

    public E2EDynamicSignedManifestEndpoint(MinisignManifestSigner signer)
    {
        _signer = signer;
    }

    public override void Configure()
    {
        Get("api/e2eSigDyn/{Product}/{Filename}");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(E2EDynamicSignedManifestRequest req, CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        if (!_signer.IsConfigured)
        {
            await Send.ErrorsAsync(503, ct);
            return;
        }

        bool isManifest = req.Filename == "updates.json";
        bool isMinisig = req.Filename == "updates.json.minisig";
        bool isDynamicSigned = req.Product == "DynamicSignedManifest";
        bool isDynamicTampered = req.Product == "DynamicTamperedManifest";

        if (!isManifest && !isMinisig)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        if (!isDynamicSigned && !isDynamicTampered)
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

        // Build the canonical UpdateResponse. PublishedAt is pinned so that the manifest bytes
        // are deterministic across manifest and minisig requests within one server run.
        string checksum = E2EGuard.ComputeSha256(zipPath);
        string baseUrl = E2EGuard.BaseUrl(HttpContext);

        UpdateResponse canonical = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "E2E Dynamic Signing Test",
                WindowTitle = "E2E Dynamic Signed Manifest Updater",
                SignatureVerificationMode = SignatureVerificationMode.Disabled
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "E2E Dynamic Signed Update v2.0.0",
                    Version = System.Version.Parse("2.0.0"),
                    PublishedAt = new DateTimeOffset(2026, 1, 1, 0, 0, 0, TimeSpan.Zero),
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

        // Serialize once; all requests (manifest + minisig for either product) use these bytes
        // to compute or verify the signature.
        byte[] canonicalBytes = JsonSerializer.SerializeToUtf8Bytes(canonical, SerializerOptions);

        // The .minisig sidecar always covers the canonical (untampered) bytes.
        // For DynamicTamperedManifest the client receives a mutated body but the original signature
        // → verification fails exactly as intended.
        if (isMinisig)
        {
            byte[] sig = _signer.SignDetached(canonicalBytes);
            HttpContext.Response.ContentType = "application/octet-stream";
            HttpContext.Response.StatusCode = 200;
            await HttpContext.Response.Body.WriteAsync(sig, ct);
            return;
        }

        // Serve the manifest body. Tampered variant mutates the version string in raw JSON so
        // the change cannot be missed without touching the signature path.
        byte[] body = isDynamicTampered
            ? Encoding.UTF8.GetBytes(
                Encoding.UTF8.GetString(canonicalBytes).Replace("\"version\":\"2.0.0\"", "\"version\":\"9.9.9\""))
            : canonicalBytes;

        HttpContext.Response.ContentType = "application/json";
        HttpContext.Response.StatusCode = 200;
        await HttpContext.Response.Body.WriteAsync(body, ct);
    }
}

internal sealed class E2EDynamicSignedManifestRequest
{
    public string Product { get; set; } = string.Empty;
    public string Filename { get; set; } = string.Empty;
}
