using System.Text.RegularExpressions;

using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

internal class BthPS3UpdatesEndpointRequest
{
    [FromHeader("X-Vicius-OS-Architecture")]
    public string OsArchitecture { get; set; } = null!;
}

internal sealed partial class BthPS3UpdatesEndpoint(GitHubApiService githubApiService)
    : Endpoint<BthPS3UpdatesEndpointRequest>
{
    public override void Configure()
    {
        Get("api/nefarius/BthPS3/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    [GeneratedRegex(@"<!--[\s\S\n]*?-->")]
    private partial Regex CommentRegex();

    public override async Task HandleAsync(BthPS3UpdatesEndpointRequest req, CancellationToken ct)
    {
        Release? release = await githubApiService.GetLatestRelease("nefarius", "BthPS3");

        if (release is null)
        {
            await SendNotFoundAsync(ct);
            return;
        }

        ReleaseAsset? asset =
            release.Assets.FirstOrDefault(a =>
                a.Name.Contains(req.OsArchitecture, StringComparison.InvariantCultureIgnoreCase));

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
                ProductName = "BthPS3",
                WindowTitle = "BthPS3 Updater",
                Detection =
                    new RegistryValueConfig
                    {
                        Hive = RegistryHive.HKLM,
                        Key = @"SOFTWARE\Nefarius Software Solutions e.U.\BthPS3 Bluetooth Drivers",
                        Value = "Version"
                    }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = release.Name,
                    PublishedAt = release.CreatedAt,
                    Version = System.Version.Parse(release.TagName.Replace("setup-v", string.Empty)),
                    Summary = summary,
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