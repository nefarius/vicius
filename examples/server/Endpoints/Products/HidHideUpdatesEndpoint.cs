using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

internal sealed class HidHideUpdatesEndpoint : EndpointWithoutRequest
{
    private readonly GitHubApiService _githubApiService;

    public HidHideUpdatesEndpoint(GitHubApiService githubApiService)
    {
        _githubApiService = githubApiService;
    }

    public override void Configure()
    {
        Get("api/nefarius/HidHide/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        Release? release = await _githubApiService.GetLatestRelease("nefarius", "HidHide");

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

        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "HidHide",
                WindowTitle = "HidHide Updater",
                Detection = new RegistryValueConfig
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
                    Summary = release.Body,
                    DownloadUrl = asset.BrowserDownloadUrl,
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