using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: signed manifest served from pre-generated files on disk.
///     Handles both the manifest JSON and the <c>.minisig</c> sidecar for both
///     <c>SignedManifest</c> (valid signature) and <c>TamperedManifest</c>
///     (tampered body, original signature — so verification fails).
///     <para>
///         Routes handled (manufacturer <c>e2eSig</c>):
///         <list type="bullet">
///             <item><c>GET api/e2eSig/{product}/updates.json</c></item>
///             <item><c>GET api/e2eSig/{product}/updates.json.minisig</c></item>
///         </list>
///     </para>
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2ESignedManifestEndpoint : Endpoint<E2ESignedManifestRequest>
{
    public override void Configure()
    {
        Get("api/e2eSig/{Product}/{Filename}");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(E2ESignedManifestRequest req, CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string artifactsDir = E2EGuard.ArtifactsDir;
        if (string.IsNullOrEmpty(artifactsDir))
            ThrowError("E2E_ARTIFACTS_DIR is not set", 500);

        // Guard: only allow the two known products and the two known filenames.
        bool isManifest = req.Filename == "updates.json";
        bool isMinisig = req.Filename == "updates.json.minisig";
        if (!isManifest && !isMinisig)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        // Both SignedManifest and TamperedManifest requests are handled here.
        // For the .minisig sidecar of TamperedManifest we intentionally serve
        // the ORIGINAL signature so verification fails against the tampered body.
        string filePath = (req.Product, isMinisig) switch
        {
            ("SignedManifest", false) => Path.Combine(artifactsDir, "SignedManifest", "updates.json"),
            ("SignedManifest", true) => Path.Combine(artifactsDir, "SignedManifest", "updates.json.minisig"),
            ("TamperedManifest", false) => Path.Combine(artifactsDir, "TamperedManifest", "updates.json"),
            // Serve the VALID signature against the TAMPERED body → verification fails
            ("TamperedManifest", true) => Path.Combine(artifactsDir, "SignedManifest", "updates.json.minisig"),
            _ => string.Empty
        };

        if (string.IsNullOrEmpty(filePath) || !File.Exists(filePath))
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string contentType = isMinisig ? "application/octet-stream" : "application/json";
        HttpContext.Response.ContentType = contentType;
        HttpContext.Response.StatusCode = 200;
        await using FileStream fs = File.OpenRead(filePath);
        await fs.CopyToAsync(HttpContext.Response.Body, ct);
    }
}

internal sealed class E2ESignedManifestRequest
{
    public string Product { get; set; } = string.Empty;
    public string Filename { get; set; } = string.Empty;
}
