using System.Diagnostics.CodeAnalysis;
using System.Text.RegularExpressions;

using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

/// <summary>
///     Crafts update configuration for
///     <a href="https://github.com/fredemmott/OpenXR-API-Layers-GUI">OpenXR API Layers GUI</a>.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
internal sealed partial class OpenXRUpdatesEndpoint(
    GitHubApiService githubApiService,
    ILogger<OpenXRUpdatesEndpoint> logger) : EndpointWithoutRequest
{
    /// <summary>
    ///     Strips leading characters and zeroes from tag name to conform to SemVer conversion.
    /// </summary>
    [GeneratedRegex(@"^v|(?<=\D)0+(?=\d)")]
    private static partial Regex TagStripRegex();

    public override void Configure()
    {
        Get("api/fredemmott/OpenXR-API-Layers-GUI/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        IEnumerable<Release>? releases = await githubApiService.GetAllReleases("fredemmott", "OpenXR-API-Layers-GUI");

        if (releases is null)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "OpenXR API Layers GUI",
                WindowTitle = "OpenXR API Layers GUI Update",
                RunAsTemporaryCopy = true
            },
            Releases = releases
                .Select(release =>
                {
                    ReleaseAsset? asset = release.Assets.FirstOrDefault();

                    if (asset is null)
                    {
                        logger.LogWarning("Release {Release} has no assets, skipping", release);
                        return null;
                    }

                    string strippedTag = TagStripRegex().Replace(release.TagName, string.Empty);

                    if (!System.Version.TryParse(strippedTag, out Version? version))
                    {
                        logger.LogWarning("Failed to parse version from tag {Tag} for release {Release}, skipping",
                            release.TagName, release);
                        return null;
                    }

                    return new UpdateRelease
                    {
                        Name = release.Name,
                        Version = version,
                        Summary = release.Body,
                        PublishedAt = release.CreatedAt,
                        DownloadUrl = asset.BrowserDownloadUrl,
                        // NOTE: this is the default value on the client, it's set here for demonstration purposes
                        ZipExtractDefaultFileDisposition = ZipExtractFileDisposition.CreateOrReplace
                    };
                })
                .Where(y => y != null)
                .Cast<UpdateRelease>()
                .ToList()
        };

        await Send.OkAsync(response, ct);
    }
}