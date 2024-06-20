using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Shared;

internal static class Examples
{
    /// <summary>
    ///     A very minimal release with only the required properties populated.
    /// </summary>
    public static UpdateRelease MinimalDemoRelease => new()
    {
        Name = "Demo",
        Summary = "Demo",
        Version = Version.Parse("2.0.0"),
        DownloadUrl = "https://example.com",
        PublishedAt = DateTimeOffset.Now
    };
}