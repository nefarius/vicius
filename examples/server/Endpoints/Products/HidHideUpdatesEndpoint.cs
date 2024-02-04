using System.Text.RegularExpressions;

using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

/// <summary>
///     Crafts update configuration for <a href="https://github.com/nefarius/HidHide">HidHide</a>. 
/// </summary>
internal sealed partial class HidHideUpdatesEndpoint(GitHubApiService githubApiService) : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/nefarius/HidHide/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    [GeneratedRegex(@"<!--[\s\S\n]*?-->")]
    private partial Regex CommentRegex();

    public override async Task HandleAsync(CancellationToken ct)
    {
        Release? release = await githubApiService.GetLatestRelease("nefarius", "HidHide");

        if (release is null)
        {
            await SendNotFoundAsync(ct);
            return;
        }

        ReleaseAsset? asset = release.Assets.FirstOrDefault();

        if (asset is null)
        {
            await SendNotFoundAsync(ct);
            return;
        }

        // strips out comment blocks and redundant newlines
        string summary = CommentRegex().Replace(release.Body, string.Empty).Trim('\r', '\n');

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "HidHide",
                WindowTitle = "HidHide Updater",
                Detection =
                    new RegistryValueConfig
                    {
                        Hive = RegistryHive.HKLM,
                        Key = @"SOFTWARE\Nefarius Software Solutions e.U.\HidHide",
                        Value = "Version"
                    }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = release.Name,
                    PublishedAt = release.CreatedAt,
                    Version = System.Version.Parse(release.TagName.Replace("v", string.Empty)),
                    Summary = summary,
                    DownloadUrl = asset.BrowserDownloadUrl,
                    LaunchArguments = "/exebasicui /qb",
                    ExitCode = new ExitCodeCheck
                    {
                        SuccessCodes =
                        {
                            0, // regular success
                            3010 // success, but reboot required
                        }
                    }
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}