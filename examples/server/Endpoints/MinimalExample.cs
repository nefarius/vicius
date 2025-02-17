﻿using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demos a simple single release update configuration with registry-based product version detection.
/// </summary>
internal sealed class MinimalExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/Minimal/updates.json");
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
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-3),
                    Version = System.Version.Parse("9.0.14"),
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
                                
                              ![Demo Logo](https://nefarius.at/wp-content/uploads/nefarius-logo-website.png)
                              """,
                    // pulling bigger (~50MB) .NET runtime setup as an example to demo progress bar
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/f9ea536d-8e1f-4247-88b8-e79e33fa0873/c06e39f73a3bb1ec8833bb1cde98fce3/windowsdesktop-runtime-7.0.12-win-x64.exe",
                    LaunchArguments = "/norestart",
                    ExitCode = new ExitCodeCheck
                    {
                        SuccessCodes =
                        {
                            0, // regular success
                            3010 // success, but reboot required
                            // 1602 // failure (user cancelled) - you normally wouldn't want that as a success code, just an example
                        }
                    }
                }
            }
        };

        await SendOkAsync(response, ct);
    }
}