using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;

using Nefarius.Vicius.Example.Server.Services;

namespace Nefarius.Vicius.Example.Server.Tests;

public sealed class ServerFactory : WebApplicationFactory<Program>
{
    public ServerFactory()
    {
        // E2E-only routes must stay dark unless a dedicated test opts in.
        Environment.SetEnvironmentVariable("VICIUS_E2E", null);
    }

    internal FakeGitHubApiService GitHub { get; } = new();

    protected override void ConfigureWebHost(IWebHostBuilder builder)
    {
        builder.UseEnvironment("Development");
        builder.ConfigureServices(services =>
        {
            services.RemoveAll<IGitHubApiService>();
            services.AddSingleton<IGitHubApiService>(GitHub);
        });
    }
}
