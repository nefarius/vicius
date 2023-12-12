using Microsoft.Extensions.Caching.Memory;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Services;

internal sealed class GitHubApiService
{
    private readonly IMemoryCache _memoryCache;
    private readonly GitHubClient _gitHubClient;

    public GitHubApiService(IMemoryCache memoryCache)
    {
        _memoryCache = memoryCache;

        _gitHubClient = new GitHubClient(new ProductHeaderValue("Nefarius.Vicius.Server"));
    }

    /// <summary>
    ///     Gets the latest release of a given GitHub repository.
    /// </summary>
    /// <param name="owner">The username or organisation.</param>
    /// <param name="name">The repository name.</param>
    /// <returns>A <see cref="Release"/> on success or null.</returns>
    public async Task<Release?> GetLatestRelease(string owner, string name)
    {
        string key = $"{owner}/{name}";

        if (_memoryCache.TryGetValue(key, out Release? cached))
        {
            return cached;
        }

        Release? release = await _gitHubClient.Repository.Release.GetLatest(owner, name);

        _memoryCache.Set(key, release,
            new MemoryCacheEntryOptions() { AbsoluteExpirationRelativeToNow = TimeSpan.FromHours(1) });

        return release;
    }
}