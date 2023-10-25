using FastEndpoints;

using Nefarius.Vicius.Example.Server.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

internal sealed class Example01Endpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/Example01/updates.json");
        AllowAnonymous();
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                LatestVersion = "1.0.0",
                LatestUrl = "https://downloads.nefarius.at/other/nefarius/vpatch/vpatch.exe"
            },
            Shared = new SharedConfig
            {
                ProductName = ".NET Runtime",
                WindowTitle = ".NET Runtime Updater",
                DetectionMethod = ProductVersionDetectionMethod.RegistryValue,
                Detection = new RegistryValueConfig()
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
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-3),
                    Version = System.Version.Parse("7.0.12"),
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
                    // pulling bigger .NET runtime setup as an example to demo progress bar
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/f9ea536d-8e1f-4247-88b8-e79e33fa0873/c06e39f73a3bb1ec8833bb1cde98fce3/windowsdesktop-runtime-7.0.12-win-x64.exe",
                    LaunchArguments = "/norestart",
                    ExitCode = new ExitCodeCheck { SkipCheck = false, SuccessCodes =
                    {
                        0, // regular success
                        3010, // success, but reboot required
                        1602 // failure (user cancelled)
                    } }
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}