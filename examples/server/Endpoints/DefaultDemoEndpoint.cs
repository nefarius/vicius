using FastEndpoints;

using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Shared;

namespace Nefarius.Vicius.Example.Server.Endpoints;

/// <summary>
///     Default happy-path showcase endpoint used for local debug runs.
///     Exercises all UI surfaces: forced-outdated detection, rich Markdown changelog
///     (headings / lists / link / image / scrollbars), help button, remind-me-tomorrow
///     button, large download with progress bar, strict Authenticode publisher-pin
///     verification (Required + Strict, pinned to "Microsoft Corporation"),
///     and a successful install+exit-code flow.
/// </summary>
internal sealed class DefaultDemoEndpoint : EndpointWithoutRequest
{
    // The server converts the PNG to a single-entry ICO (PNG-compressed, Vista+ compatible)
    // and base64-encodes it once at startup.  The client receives a standard .ico buffer and
    // does not need to know anything about PNG – it simply calls CreateIconFromResourceEx.
    private static readonly string DemoIconBase64 = LoadDemoIconBase64();

    private static string LoadDemoIconBase64()
    {
        // The PNG is embedded as a resource in server.csproj so no file-system path is needed.
        using var stream = typeof(DefaultDemoEndpoint).Assembly
            .GetManifestResourceStream("demo-icon.png");

        if (stream is null)
            return string.Empty;

        var png = new byte[stream.Length];
        _ = stream.Read(png, 0, png.Length);

        return IconConverter.PngToIcoBase64(png);
    }

    public override void Configure()
    {
        // Pointed at by the committed vcxproj.user --server-url arg.
        // The second route covers a plain Debug build whose tenant path resolves to "Updater".
        Get("api/demo/Showcase/updates.json", "api/Updater/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("Examples"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                // Shows the help button (top-left of the start page) and opens a URL when clicked.
                HelpUrl = "https://docs.nefarius.at/projects/Vicius/",
                ExitCode = new ExitCodeCheck
                {
                    SuccessCodes =
                    {
                        0,    // regular success
                        3010  // success, reboot required
                    }
                }
            },
            Shared = new SharedConfig
            {
                ProductName = "Vicius Demo Product",
                WindowTitle = "Vicius Demo Updater",
                // FixedVersionConfig with a very low version forces "outdated" on every machine,
                // so the full update wizard is always shown regardless of the test environment.
                Detection = new FixedVersionConfig
                {
                    Version = "0.0.1"
                },
                // HideRemindButton intentionally left unset → "Remind me tomorrow" button is visible.

                // Custom taskbar / title-bar icon.
                // The server wraps the PNG asset in a one-entry ICO container (PNG-compressed,
                // Windows Vista+ compatible) and base64-encodes it.  The client only ever sees a
                // standard .ico buffer, so it stays simple.  An absent or invalid value is a
                // silent no-op: the compiled-in icon resource is used as fallback.
                IconBase64 = DemoIconBase64.Length > 0 ? DemoIconBase64 : null,

                // Demonstrate full strict publisher-pin verification using the Microsoft-signed download.
                // Required  → unsigned setups are rejected (chain validation is mandatory).
                // Strict    → the subject name pin must match; a mismatch is a hard failure.
                // SubjectName ".NET" and IssuerName "Microsoft Code Signing PCA 2011" are the
                // actual values on the .NET Desktop Runtime installer cert (confirmed from logs).
                // SubjectName is stable across renewals; IssuerName is semi-stable but provides
                // an extra layer to demonstrate multi-field pinning in the demo.
                SignatureVerificationMode = SignatureVerificationMode.Required,
                SignaturePolicy = SignatureComparisonPolicy.Strict,
                SignatureConfig = new SignatureConfig
                {
                    SubjectName = ".NET",
                    IssuerName = "Microsoft Code Signing PCA 2011"
                }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Vicius Demo Update v99.0.0",
                    PublishedAt = DateTimeOffset.UtcNow.AddDays(-1),
                    Version = System.Version.Parse("99.0.0"),
                    // Rich Markdown exercises: H2 headings, long bullet list (vertical scrollbar),
                    // hyperlink, inline image.
                    Summary = """
                              ## What's new

                              This is the **default demo release** shipped with the example server.
                              It is designed to exercise every UI surface of the updater wizard
                              during a local debug run.

                              ## Feature highlights

                              - Rich Markdown changelog rendering (what you are reading right now)
                              - H1 / H2 / H3 heading support
                              - **Bold**, *italic*, and `inline code` text formatting
                              - Hyperlink: [Vicius documentation](https://docs.nefarius.at/projects/Vicius/)
                              - Inline image (logo below)
                              - Long enough to need a vertical scroll bar — keep reading!
                              - Bullet list with many items to trigger scrollbars
                              - Download progress bar (large real-world installer below)
                              - Authenticode publisher-pin verification (Required + Strict, pinned to subject ".NET" / issuer "Microsoft Code Signing PCA 2011")
                              - Help button (top of start page, opens the Vicius docs)
                              - Remind me tomorrow button (start page)
                              - Back / Cancel navigation
                              - Successful exit-code mapping (0 and 3010 both count as success)

                              ## Scrollbar test

                              - Item 1
                              - Item 2
                              - Item 3
                              - Item 4
                              - Item 5
                              - Item 6
                              - Item 7
                              - Item 8
                              - Item 9
                              - Item 10

                              ## Demo logo

                              ![Nefarius logo](https://nefarius.at/wp-content/uploads/nefarius-logo-website.png)

                              ## More details

                              The download below is the **genuine, Microsoft-signed** .NET Desktop Runtime
                              installer (~50 MB). It is used purely to drive the download progress bar and
                              the Authenticode verification step. The installer is launched with `/norestart`
                              so it exits quickly without modifying the system in a meaningful way.

                              [Release notes on Microsoft Learn](https://learn.microsoft.com/dotnet/core/whats-new/)
                              """,
                    // ~50 MB Microsoft-signed binary → drives progress bar + passes Required/Strict publisher-pin check.
                    DownloadUrl =
                        "https://download.visualstudio.microsoft.com/download/pr/bb581716-4cca-466e-9857-512e2371734b/5fe261422a7305171866fd7812d0976f/windowsdesktop-runtime-8.0.7-win-x64.exe",
                    LaunchArguments = "/norestart",
                    ExitCode = new ExitCodeCheck
                    {
                        SuccessCodes =
                        {
                            0,    // regular success
                            3010  // success, reboot required
                        }
                    }
                }
            }
        };

        await Send.OkAsync(response, ct);
    }
}
