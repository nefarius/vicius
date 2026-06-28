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
            await SendNotFoundAsync(ct);
            return;
        }

        string artifactsDir = E2EGuard.ArtifactsDir;
        if (string.IsNullOrEmpty(artifactsDir))
        {
            AddError("E2E_ARTIFACTS_DIR environment variable is not set");
            await SendErrorsAsync(500, ct);
            return;
        }

        // Sanitize: reject any path containing directory separators so only
        // flat filenames in the artifacts root can be requested.
        if (req.Name.Contains('/') || req.Name.Contains('\\') || req.Name.Contains(".."))
        {
            await SendNotFoundAsync(ct);
            return;
        }

        string filePath = Path.Combine(artifactsDir, req.Name);

        if (!File.Exists(filePath))
        {
            await SendNotFoundAsync(ct);
            return;
        }

        string contentType = Path.GetExtension(req.Name).ToLowerInvariant() switch
        {
            ".zip" => "application/zip",
            ".exe" => "application/octet-stream",
            _ => "application/octet-stream"
        };

        await SendStreamAsync(File.OpenRead(filePath), contentType: contentType, cancellation: ct);
    }
}

internal sealed class E2EArtifactRequest
{
    public string Name { get; set; } = string.Empty;
}
