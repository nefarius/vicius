using Nefarius.Vicius.Example.Server.Services;

using Octokit;

namespace Nefarius.Vicius.Example.Server.Tests;

internal sealed class FakeGitHubApiService : IGitHubApiService
{
    public Release? LatestRelease { get; set; }

    public IEnumerable<Release>? AllReleases { get; set; }

    public Task<Release?> GetLatestRelease(string owner, string name) =>
        Task.FromResult(LatestRelease);

    public Task<IEnumerable<Release>?> GetAllReleases(string owner, string name) =>
        Task.FromResult(AllReleases);
}
