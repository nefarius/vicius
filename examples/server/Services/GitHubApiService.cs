using Microsoft.Extensions.Caching.Memory;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Abstracts calls to GitHub REST API and caches them to avoid hitting rate limits.
/// </summary>
internal sealed class GitHubApiService
{
    private readonly IHostEnvironment _environment;
    private readonly GitHubClient _gitHubClient;
    private readonly IMemoryCache _memoryCache;

    /// <summary>
    ///     Abstracts calls to GitHub REST API and caches them to avoid hitting rate limits.
    /// </summary>
    public GitHubApiService(IMemoryCache memoryCache, IHostEnvironment environment, IConfiguration configuration)
    {
        _memoryCache = memoryCache;
        _environment = environment;

        _gitHubClient = new GitHubClient(new ProductHeaderValue("Nefarius.Vicius.Server"));

        string? token = configuration.GetSection("GitHub:Token").Get<string>();

        if (!string.IsNullOrEmpty(token))
        {
            _gitHubClient.Credentials = new Credentials(token, AuthenticationType.Bearer);
        }
    }

    /// <summary>
    ///     Gets the latest release of a given GitHub repository.
    /// </summary>
    /// <param name="owner">The username or organisation.</param>
    /// <param name="name">The repository name.</param>
    /// <returns>A <see cref="Release" /> on success or null.</returns>
    public async Task<Release?> GetLatestRelease(string owner, string name)
    {
        string key = $"{nameof(GitHubApiService)}-{owner}/{name}";

        if (!_environment.IsDevelopment())
        {
            if (_memoryCache.TryGetValue(key, out Release? cached))
            {
                return cached;
            }
        }

        Release? release = await _gitHubClient.Repository.Release.GetLatest(owner, name);

        if (!_environment.IsDevelopment())
        {
            _memoryCache.Set(
                key,
                release,
                new MemoryCacheEntryOptions { AbsoluteExpirationRelativeToNow = TimeSpan.FromHours(1) }
            );
        }

        return release;
    }

    /// <summary>
    ///     Gets all releases of a given GitHub repository.
    /// </summary>
    /// <param name="owner">The username or organisation.</param>
    /// <param name="name">The repository name.</param>
    /// <returns>A list of <see cref="Release" />s on success or null.</returns>
    public async Task<IEnumerable<Release>?> GetAllReleases(string owner, string name)
    {
        string key = $"{nameof(GitHubApiService)}-{owner}/{name}";

        if (!_environment.IsDevelopment())
        {
            if (_memoryCache.TryGetValue(key, out IReadOnlyList<Release>? cached))
            {
                return cached;
            }
        }

        IReadOnlyList<Release>? releases = await _gitHubClient.Repository.Release.GetAll(owner, name);

        if (!_environment.IsDevelopment())
        {
            _memoryCache.Set(
                key,
                releases,
                new MemoryCacheEntryOptions { AbsoluteExpirationRelativeToNow = TimeSpan.FromHours(1) }
            );
        }

        return releases;
    }
}