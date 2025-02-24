using System.Diagnostics.CodeAnalysis;
using System.Text.RegularExpressions;

using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
internal class BthPS3UpdatesEndpointRequest
{
    /// <summary>
    ///     Gets the Operating System CPU architecture the updater is running on.
    ///     Possible values are: <c>x64</c> for Intel/AMD 64-Bit, <c>arm64</c> for ARM 64-Bit or <c>x86</c> for Intel/AMD
    ///     32-Bit.
    /// </summary>
    /// <example>x64</example>
    [FromHeader("X-Vicius-OS-Architecture")]
    public string OsArchitecture { get; set; } = null!;
}

/// <summary>
///     Crafts update configuration for <a href="https://github.com/nefarius/BthPS3">BthPS3</a>.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
internal sealed partial class BthPS3UpdatesEndpoint(GitHubApiService githubApiService)
    : Endpoint<BthPS3UpdatesEndpointRequest>
{
    public override void Configure()
    {
        Get("api/nefarius/BthPS3/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    /// <summary>
    ///     Strips HTML-style comment blocks from Markdown body. This was used in the past to carry metadata for the Advanced
    ///     Installer Updater proxy.
    /// </summary>
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
                        Key = @"SOFTWARE\Nefarius Software Solutions e.U.\Nefarius BthPS3 Bluetooth Drivers",
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
                    DownloadSize = asset.Size,
                    LaunchArguments = """
                                      FILTERNOTFOUND="1"
                                      """,
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