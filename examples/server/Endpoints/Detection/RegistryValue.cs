using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints.Detection;

/// <summary>
///     Demos product detection using a registry query.
/// </summary>
internal sealed class RegistryValueEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/RegistryValue/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                HelpUrl = "https://docs.nefarius.at/projects/Vicius/Examples/Help-Page/"
            },
            Shared = new SharedConfig
            {
                ProductName = "HidHide",
                WindowTitle = "HidHide Updater",
                // this example the value of a registry key to get the local version
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
                    Name = "HidHide Update",
                    PublishedAt = DateTimeOffset.Parse("2023-11-02"),
                    Version = System.Version.Parse("1.4.186"),
                    Summary = """
                              ## How to install

                              Download the provided setup and click through it. Reboot when prompted to. Done!

                              ## How to update

                              Uninstall the existing version, reboot. Then install this version, reboot. You're done!

                              ## Remarks

                              This release supports **Windows 10/11 64-Bit Intel/AMD only**.

                              ## Changes

                              - Fixed broken path generation in updater config file completely breaking the updater. Thanks a ton, "Advanced" Installer...
                              - Replaced update server URL to mitigate [ViGEm infrastructure EOL](https://docs.nefarius.at/projects/ViGEm/End-of-Life/)
                              - Updated support and other URLs in setup
                              - Upgraded everything to Visual Studio 2022 and Windows 11 SDK/WDK
                              - Added watchdog Windows service to protect against broken filter configurations (#96)
                                - This should eradicate the need to reinstall HidHide e.g. after a Windows Update silently broke it
                              - Updated the [FAQ entries](https://docs.nefarius.at/projects/HidHide/FAQ/#how-to-fix-this-package-can-only-be-run-from-a-bootstrapper-message)
                              - Fixed an issue when upgrading with the stupid setup project ([see](https://docs.nefarius.at/projects/HidHide/FAQ/#how-to-fix-this-package-can-only-be-run-from-a-bootstrapper-message))
                              - Added WTS session jail support via backwards-compatible API extension (#125) thanks to @Black-Seraph

                              **Full Changelog**: https://github.com/nefarius/HidHide/compare/v1.2.128.0...v1.4.186.0
                              """,
                    DownloadUrl =
                        "https://github.com/nefarius/HidHide/releases/download/v1.4.186.0/HidHide_1.4.186_x64.exe",
                    LaunchArguments = "/norestart",
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

        await Send.OkAsync(response, ct);
    }
}