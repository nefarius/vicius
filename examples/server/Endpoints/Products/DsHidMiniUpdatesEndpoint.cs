using System.Diagnostics.CodeAnalysis;
using System.Text.Json;
using System.Text.RegularExpressions;

using FastEndpoints;

using Microsoft.Extensions.Caching.Memory;

using Nefarius.Vicius.Abstractions.Models;
using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Endpoints.Products;

[SuppressMessage("ReSharper", "InconsistentNaming")]
[SuppressMessage("ReSharper", "ClassNeverInstantiated.Global")]
internal class DsHidMiniUpdatesEndpointRequest
{
    /// <summary>
    ///     Gets the Operating System CPU architecture the updater is running on.
    ///     Supported values are: <c>x64</c> for Intel/AMD 64-Bit or <c>arm64</c> for ARM 64-Bit.
    ///     Optional for legacy clients that predate this header; defaults to <c>x64</c> when absent.
    ///     32-Bit (<c>x86</c>) and any other value return 404 — DsHidMini ships a combined x64/ARM64 MSI only.
    /// </summary>
    /// <example>x64</example>
    [FromHeader("X-Vicius-OS-Architecture", isRequired: false)]
    public string OsArchitecture { get; set; } = "x64";

    public string Filename { get; set; } = string.Empty;
}

/// <summary>
///     Crafts update configuration for <a href="https://github.com/nefarius/DsHidMini">DsHidMini</a>.
///     Serves both <c>updates.json</c> and the optional detached <c>updates.json.minisig</c>
///     sidecar (same serialized bytes) when <see cref="MinisignManifestSigner" /> is configured.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
internal sealed partial class DsHidMiniUpdatesEndpoint(
    IGitHubApiService githubApiService,
    MinisignManifestSigner signer,
    IMemoryCache cache)
    : Endpoint<DsHidMiniUpdatesEndpointRequest>
{
    private static readonly TimeSpan CacheDuration = TimeSpan.FromHours(1);

    public override void Configure()
    {
        Get("api/nefarius/DsHidMini/{Filename}");
        AllowAnonymous();
        Options(x => x.WithTags("Production"));
    }

    /// <summary>
    ///     Strips HTML-style comment blocks from Markdown body. This was used in the past to carry metadata for the Advanced
    ///     Installer Updater proxy.
    /// </summary>
    [GeneratedRegex(@"<!--[\s\S\n]*?-->")]
    private partial Regex CommentRegex();

    public override async Task HandleAsync(DsHidMiniUpdatesEndpointRequest req, CancellationToken ct)
    {
        bool isManifest = req.Filename == "updates.json";
        bool isMinisig = req.Filename == "updates.json.minisig";

        if (!isManifest && !isMinisig)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        if (isMinisig && !signer.IsConfigured)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        string arch = string.IsNullOrWhiteSpace(req.OsArchitecture)
            ? "x64"
            : req.OsArchitecture.Trim().ToLowerInvariant();

        // One snapshot per architecture (JSON + sidecar) so both routes serve the same
        // bytes, including in Development and across a GitHub cache refresh.
        // Concurrent misses share one GitHub lookup and one signed snapshot.
        string cacheKey = $"DsHidMiniUpdates:{arch}";
        CachedManifest? cached = await ManifestSnapshotCache.GetOrCreateAsync(
            cache,
            cacheKey,
            CacheDuration,
            () => TryBuildSnapshotAsync(arch));
        if (cached is null)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        HttpContext.Response.Headers.CacheControl = "public, max-age=3600";

        if (isMinisig)
        {
            if (cached.Minisig is null)
            {
                await Send.NotFoundAsync(ct);
                return;
            }

            HttpContext.Response.ContentType = "application/octet-stream";
            HttpContext.Response.StatusCode = 200;
            await HttpContext.Response.Body.WriteAsync(cached.Minisig, ct);
            return;
        }

        HttpContext.Response.ContentType = "application/json";
        HttpContext.Response.StatusCode = 200;
        await HttpContext.Response.Body.WriteAsync(cached.Json, ct);
    }

    private sealed record CachedManifest(byte[] Json, byte[]? Minisig);

    private async Task<CachedManifest?> TryBuildSnapshotAsync(string arch)
    {
        // Combined x64/ARM64 MSI only; anything else (including x86) is unsupported.
        if (arch is not ("x64" or "arm64"))
            return null;

        Release? release = await githubApiService.GetLatestRelease("nefarius", "DsHidMini");
        if (release is null)
            return null;

        ReleaseAsset? asset =
            release.Assets.FirstOrDefault(a =>
                a.Name.Contains(arch, StringComparison.InvariantCultureIgnoreCase));
        if (asset is null)
            return null;

        byte[] json = JsonSerializer.SerializeToUtf8Bytes(
            BuildResponse(release, asset), ManifestJson.SerializerOptions);
        byte[]? minisig = signer.IsConfigured ? signer.SignDetached(json) : null;
        return new CachedManifest(json, minisig);
    }

    private UpdateResponse BuildResponse(Release release, ReleaseAsset asset)
    {
        // strips out comment blocks and redundant newlines
        string summary = CommentRegex().Replace(release.Body, string.Empty).Trim('\r', '\n');

        return new UpdateResponse
        {
            Shared = new SharedConfig
            {
                ProductName = "DsHidMini",
                WindowTitle = "DsHidMini Updater",
                Detection =
                    new RegistryValueConfig
                    {
                        Hive = RegistryHive.HKLM,
                        Key = @"SOFTWARE\Nefarius Software Solutions e.U.\Nefarius DsHidMini Driver",
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
    }
}
