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
internal sealed partial class OpenXRUpdatesEndpoint(GitHubApiService githubApiService) : EndpointWithoutRequest
{
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
            await SendNotFoundAsync(ct);
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
                        return null;
                    }

                    Version version = System.Version.Parse(TagStripRegex().Replace(release.TagName, string.Empty));

                    return new UpdateRelease
                    {
                        Name = release.Name,
                        Version = version,
                        Summary = release.Body,
                        PublishedAt = release.CreatedAt,
                        DownloadUrl = asset.BrowserDownloadUrl
                    };
                })
                .Where(y => y != null)
                .Cast<UpdateRelease>()
                .ToList()
        };

        await SendOkAsync(response, ct);
    }
}