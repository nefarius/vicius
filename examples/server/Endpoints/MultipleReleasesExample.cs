using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demos an update providing multiple releases.
/// </summary>
internal sealed class MultipleReleasesExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/Multiple/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = ".NET Runtime",
                WindowTitle = ".NET Runtime Updater",
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKLM,
                    Key = @"SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedhost",
                    Value = "Version"
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = ".NET Runtime Demo Update",
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-2),
                    Version = System.Version.Parse("8.0.7"),
                    // Markdown support in summary (changelog, description)
                    Summary = """
                              ## Features
                                * Awesome new thing
                                * Also this
                                * And that
                                
                              ## Bugfixes
                                * Removed bug I put there
                                * Whoops, that wasn't supposed to happen
                                
                              ## Nonsense
                                * Need to see
                                * What happens...
                                * If we need scroll bars
                                * Vertically
                                * [Links are also supported!](https://example.org)
                              """,
                    // pulling bigger (~50MB) .NET runtime setup as an example to demo progress bar
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/bb581716-4cca-466e-9857-512e2371734b/5fe261422a7305171866fd7812d0976f/windowsdesktop-runtime-8.0.7-win-x64.exe"
                },
                new UpdateRelease
                {
                    Name = ".NET Runtime Demo Update",
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-3),
                    Version = System.Version.Parse("7.0.14"),
                    // Markdown support in summary (changelog, description)
                    Summary = """
                              ## Features
                                * Awesome new thing
                                * Also this
                                * And that
                                
                              ## Bugfixes
                                * Removed bug I put there
                                * Whoops, that wasn't supposed to happen
                                
                              ## Nonsense
                                * Need to see
                                * What happens...
                                * If we need scroll bars
                                * Vertically
                                * [Links are also supported!](https://example.org)
                              """,
                    // pulling bigger (~50MB) .NET runtime setup as an example to demo progress bar
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/f9ea536d-8e1f-4247-88b8-e79e33fa0873/c06e39f73a3bb1ec8833bb1cde98fce3/windowsdesktop-runtime-7.0.12-win-x64.exe"
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}