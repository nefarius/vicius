using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demos behavioral changes when <see cref="SharedConfig.RunAsTemporaryCopy" /> is set.
/// </summary>
internal sealed class TempCopyExample : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/TempCopy/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "Temporary Copy Demo",
                WindowTitle = "Temporary Copy Demo Updater",
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKLM,
                    Key = @"SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedhost",
                    Value = "Version"
                },
                RunAsTemporaryCopy = true
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = ".NET Runtime Demo Update",
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-3),
                    Version = System.Version.Parse("7.0.14"),
                    // Markdown support in summary (changelog, description)
                    Summary = """
                              ## Heading
                              Fill me with Markdown!
                              """,
                    // pulling bigger (~50MB) .NET runtime setup as an example to demo progress bar
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/f9ea536d-8e1f-4247-88b8-e79e33fa0873/c06e39f73a3bb1ec8833bb1cde98fce3/windowsdesktop-runtime-7.0.12-win-x64.exe",
                    LaunchArguments = "/norestart",
                    ExitCode = new ExitCodeCheck { SuccessCodes = { 0 } }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}