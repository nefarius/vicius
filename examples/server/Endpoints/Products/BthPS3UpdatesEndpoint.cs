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
internal class BthPS3UpdatesEndpointRequest
{
    /// <summary>
    ///     Gets the Operating System CPU architecture the updater is running on.
    ///     Possible values are: <c>x64</c> for Intel/AMD 64-Bit, <c>arm64</c> for ARM 64-Bit or <c>x86</c> for Intel/AMD
    ///     32-Bit.
    ///     Optional for legacy clients that predate this header; defaults to <c>x64</c> when absent.
    /// </summary>
    /// <example>x64</example>
    [FromHeader("X-Vicius-OS-Architecture", isRequired: false)]
    public string OsArchitecture { get; set; } = "x64";

    public string Filename { get; set; } = string.Empty;
}

/// <summary>
///     Crafts update configuration for <a href="https://github.com/nefarius/BthPS3">BthPS3</a>.
///     Serves both <c>updates.json</c> and the optional detached <c>updates.json.minisig</c>
///     sidecar (same serialized bytes) when <see cref="MinisignManifestSigner" /> is configured.
/// </summary>
[SuppressMessage("ReSharper", "InconsistentNaming")]
internal sealed partial class BthPS3UpdatesEndpoint(
    IGitHubApiService githubApiService,
    MinisignManifestSigner signer,
    IMemoryCache cache,
    ILogger<BthPS3UpdatesEndpoint> logger)
    : Endpoint<BthPS3UpdatesEndpointRequest>
{
    private static readonly TimeSpan CacheDuration = TimeSpan.FromHours(1);

    public override void Configure()
    {
        Get("api/nefarius/BthPS3/{Filename}");
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
        string cacheKey = $"BthPS3Updates:{arch}";
        if (!cache.TryGetValue(cacheKey, out CachedManifest? cached) || cached is null)
        {
            cached = await TryBuildSnapshotAsync(arch);
            if (cached is null)
            {
                await Send.NotFoundAsync(ct);
                return;
            }

            cache.Set(cacheKey, cached, new MemoryCacheEntryOptions
            {
                AbsoluteExpirationRelativeToNow = CacheDuration
            });
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
        Release? release = await githubApiService.GetLatestRelease("nefarius", "BthPS3");
        if (release is null)
            return null;

        ReleaseAsset? asset =
            release.Assets.FirstOrDefault(a =>
                a.Name.Contains(arch, StringComparison.InvariantCultureIgnoreCase));
        if (asset is null)
            return null;

        if (!TryParseReleaseVersion(release.TagName, out System.Version? version))
        {
            logger.LogWarning(
                "Failed to parse version from tag {Tag} for release {Release}, skipping",
                release.TagName,
                release.Name);
            return null;
        }

        byte[] json = JsonSerializer.SerializeToUtf8Bytes(
            BuildResponse(release, asset, version), ManifestJson.SerializerOptions);
        byte[]? minisig = signer.IsConfigured ? signer.SignDetached(json) : null;
        return new CachedManifest(json, minisig);
    }

    /// <summary>
    ///     Maps a <c>setup-v</c> GitHub tag onto the numeric version required by the updater manifest.
    ///     A well-formed SemVer pre-release or build suffix is discarded, so <c>setup-v3.0.0-r6</c> becomes <c>3.0.0</c>.
    ///     A malformed suffix is rejected.
    /// </summary>
    private static bool TryParseReleaseVersion(string tagName, [NotNullWhen(true)] out System.Version? version)
    {
        version = null;
        const string prefix = "setup-v";
        if (!tagName.StartsWith(prefix, StringComparison.Ordinal))
            return false;

        string remainder = tagName[prefix.Length..];
        int suffixIndex = remainder.IndexOfAny(['-', '+']);
        if (suffixIndex >= 0 && !IsValidSemVerSuffix(remainder.AsSpan(suffixIndex)))
            return false;

        string numeric = suffixIndex >= 0 ? remainder[..suffixIndex] : remainder;
        string[] components = numeric.Split('.');
        if (components.Length is not (3 or 4))
            return false;

        if (components.Any(component => component.Length > 1 && component[0] == '0'))
            return false;

        return System.Version.TryParse(numeric, out version);
    }

    /// <summary>
    ///     A suffix is either <c>-pre.release+build.meta</c> or <c>+build.meta</c>.
    ///     Identifiers are non-empty and contain only ASCII alphanumerics and hyphens.
    ///     Numeric pre-release identifiers cannot have leading zeroes. Build metadata can.
    /// </summary>
    private static bool IsValidSemVerSuffix(ReadOnlySpan<char> suffix)
    {
        if (suffix.Length < 2 || suffix[0] is not ('-' or '+'))
            return false;

        ReadOnlySpan<char> remaining = suffix[1..];
        if (suffix[0] == '-')
        {
            int buildIndex = remaining.IndexOf('+');
            ReadOnlySpan<char> prerelease = buildIndex >= 0 ? remaining[..buildIndex] : remaining;
            if (!HasValidSemVerIdentifiers(prerelease, allowNumericLeadingZeroes: false))
                return false;

            if (buildIndex < 0)
                return true;

            remaining = remaining[(buildIndex + 1)..];
        }

        return HasValidSemVerIdentifiers(remaining, allowNumericLeadingZeroes: true);
    }

    private static bool HasValidSemVerIdentifiers(ReadOnlySpan<char> value, bool allowNumericLeadingZeroes)
    {
        if (value.IsEmpty)
            return false;

        int start = 0;
        while (true)
        {
            int relativeDot = value[start..].IndexOf('.');
            int end = relativeDot < 0 ? value.Length : start + relativeDot;
            if (!IsValidSemVerIdentifier(value[start..end], allowNumericLeadingZeroes))
                return false;

            if (relativeDot < 0)
                return true;

            start = end + 1;
        }
    }

    private static bool IsValidSemVerIdentifier(ReadOnlySpan<char> identifier, bool allowNumericLeadingZeroes)
    {
        if (identifier.IsEmpty)
            return false;

        bool numeric = true;
        foreach (char character in identifier)
        {
            if (character is >= '0' and <= '9')
                continue;

            numeric = false;
            if (character is (>= 'A' and <= 'Z') or (>= 'a' and <= 'z') or '-')
                continue;

            return false;
        }

        return allowNumericLeadingZeroes || !numeric || identifier.Length == 1 || identifier[0] != '0';
    }

    private UpdateResponse BuildResponse(Release release, ReleaseAsset asset, System.Version version)
    {
        // strips out comment blocks and redundant newlines
        string summary = CommentRegex().Replace(release.Body, string.Empty).Trim('\r', '\n');

        return new UpdateResponse
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
                    Version = version,
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
    }
}
