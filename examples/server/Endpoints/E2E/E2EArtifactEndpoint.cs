using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     Serves raw test artifact files directly from <c>E2E_ARTIFACTS_DIR</c> on disk.
///     Only active when the <c>VICIUS_E2E=1</c> environment variable is set.
/// </summary>
internal sealed class E2EArtifactEndpoint : Endpoint<E2EArtifactRequest>
{
    public override void Configure()
    {
        Get("api/e2e/artifacts/{Name}");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(E2EArtifactRequest req, CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string artifactsDir = E2EGuard.ArtifactsDir;
        if (string.IsNullOrEmpty(artifactsDir))
            ThrowError("E2E_ARTIFACTS_DIR is not set", 500);

        // Sanitize: reject separators, traversal sequences, and any rooted/drive-relative
        // name (e.g. "C:evil") that would escape artifactsDir after Path.Combine.
        if (req.Name.Contains('/') || req.Name.Contains('\\') || req.Name.Contains("..") ||
            Path.IsPathRooted(req.Name))
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string filePath = Path.Combine(artifactsDir, req.Name);

        if (!File.Exists(filePath))
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string contentType = Path.GetExtension(req.Name).ToLowerInvariant() switch
        {
            ".zip" => "application/zip",
            _ => "application/octet-stream"
        };

        HttpContext.Response.ContentType = contentType;
        HttpContext.Response.StatusCode = 200;
        await using FileStream fs = File.OpenRead(filePath);
        await fs.CopyToAsync(HttpContext.Response.Body, ct);
    }
}

internal sealed class E2EArtifactRequest
{
    public string Name { get; set; } = string.Empty;
}
