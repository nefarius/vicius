using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Demonstrates the exit-code message map feature (issue #24).
///
///     When the MSI installer exits with code 3010 (ERROR_SUCCESS_REBOOT_REQUIRED) the
///     updater normally treats it as plain success and closes silently. By adding an entry
///     to <see cref="ExitCodeCheck.Messages" /> the distributor can instead display a
///     user-visible notice explaining why a reboot is necessary, with an optional link to
///     a help article.
///
///     The <c>isSuccess: true</c> flag in the message entry promotes code 3010 to a success
///     condition, so it does not need to be repeated in <see cref="ExitCodeCheck.SuccessCodes" />.
/// </summary>
internal sealed class ExitCodeMessageExampleEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/contoso/ExitCodeMessage/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "Contoso App",
                WindowTitle = "Contoso App Updater",
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKLM,
                    Key = @"SOFTWARE\Contoso\App",
                    Value = "Version"
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Contoso App 2.0",
                    PublishedAt = DateTimeOffset.Parse("2024-06-01"),
                    Version = System.Version.Parse("2.0.0"),
                    Summary = """
                              ## Contoso App 2.0

                              This release requires a reboot to complete the installation.

                              After clicking **Finish** in the updater, please save your work and restart Windows.
                              """,
                    DownloadUrl = "https://example.com/contoso-app-2.0-setup.msi",
                    LaunchArguments = "/norestart",
                    ExitCode = new ExitCodeCheck
                    {
                        // Code 0 is implicitly the default success; 3010 is promoted to success
                        // via the Messages map entry below (IsSuccess = true) so it doesn't need
                        // to be listed here explicitly — though listing both is also valid.
                        SuccessCodes = { 0 },
                        Messages = new Dictionary<string, ExitCodeMessage>
                        {
                            // 3010 = ERROR_SUCCESS_REBOOT_REQUIRED (MSI "success, reboot required")
                            ["3010"] = new ExitCodeMessage
                            {
                                IsSuccess = true,
                                Message =
                                    "The update was installed successfully. A system reboot is required " +
                                    "before the new version becomes active. Please save your work and " +
                                    "restart Windows at your earliest convenience.",
                                HelpUrl = "https://docs.contoso.example/app/update-reboot",
                                ButtonText = "Learn more about the reboot requirement"
                            }
                        }
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
