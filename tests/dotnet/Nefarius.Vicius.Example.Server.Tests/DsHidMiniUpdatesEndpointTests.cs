using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.DependencyInjection;

using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Tests;

public sealed class DsHidMiniUpdatesEndpointTests : IClassFixture<ServerFactory>
{
    private const string Path = "/api/nefarius/DsHidMini/updates.json";

    private static readonly JsonSerializerOptions SerializerOptions = CreateSerializerOptions();

    private readonly ServerFactory _factory;

    public DsHidMiniUpdatesEndpointTests(ServerFactory factory)
    {
        _factory = factory;
        _factory.GitHub.LatestRelease = CreateCombinedArchitectureRelease();
        _factory.GitHub.AllReleases = null;
    }

    [Fact]
    public async Task Missing_architecture_header_defaults_to_x64_and_returns_200()
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);

        HttpResponseMessage response = await client.SendAsync(request);
        UpdateResponse manifest = await ReadManifest(response);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.True(response.Headers.CacheControl?.Public);
        Assert.Equal(TimeSpan.FromHours(1), response.Headers.CacheControl?.MaxAge);
        Assert.Null(request.Content);
        Assert.Equal(string.Empty, request.RequestUri?.Query);
        Assert.Equal("https://example.test/DsHidMini.msi", Assert.Single(manifest.Releases).DownloadUrl);
        Assert.Equal(new Version(3, 5, 1), manifest.Releases[0].Version);
        Assert.Equal("Visible notes", manifest.Releases[0].Summary);
        Assert.Equal("DsHidMini", manifest.Shared?.ProductName);
        RegistryValueConfig detection = Assert.IsType<RegistryValueConfig>(manifest.Shared?.Detection);
        Assert.Equal(RegistryHive.HKLM, detection.Hive);
        Assert.Equal(@"SOFTWARE\Nefarius Software Solutions e.U.\Nefarius DsHidMini Driver", detection.Key);
        Assert.Equal([0, 3010], manifest.Releases[0].ExitCode?.SuccessCodes);
    }

    [Theory]
    [InlineData("x64")]
    [InlineData("arm64")]
    public async Task Supported_architecture_header_selects_the_combined_msi(string architecture)
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);
        request.Headers.TryAddWithoutValidation("X-Vicius-OS-Architecture", architecture);

        HttpResponseMessage response = await client.SendAsync(request);
        UpdateResponse manifest = await ReadManifest(response);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal("https://example.test/DsHidMini.msi", Assert.Single(manifest.Releases).DownloadUrl);
    }

    [Theory]
    [InlineData("x86")]
    [InlineData("riscv64")]
    public async Task Unsupported_architecture_header_returns_404(string architecture)
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);
        request.Headers.TryAddWithoutValidation("X-Vicius-OS-Architecture", architecture);

        HttpResponseMessage response = await client.SendAsync(request);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Minisig_without_signer_returns_404()
    {
        HttpClient client = _factory.CreateClient();

        HttpResponseMessage response = await client.GetAsync("/api/nefarius/DsHidMini/updates.json.minisig");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Unknown_filename_returns_404()
    {
        HttpClient client = _factory.CreateClient();

        HttpResponseMessage response = await client.GetAsync("/api/nefarius/DsHidMini/notes.txt");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Missing_github_release_returns_404()
    {
        HttpClient client = _factory.CreateClient();
        if (_factory.Services.GetRequiredService<IMemoryCache>() is MemoryCache memoryCache)
            memoryCache.Clear();

        _factory.GitHub.LatestRelease = null;
        try
        {
            HttpResponseMessage response = await client.GetAsync(Path);

            Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
        }
        finally
        {
            _factory.GitHub.LatestRelease = CreateCombinedArchitectureRelease();
        }
    }

    [Fact]
    public async Task Response_omits_nulls_and_uses_string_enums()
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);

        HttpResponseMessage response = await client.SendAsync(request);
        JsonObject root = JsonNode.Parse(await response.Content.ReadAsStringAsync())!.AsObject();

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.False(root.ContainsKey("instance"));
        Assert.Equal("RegistryValue", root["shared"]?["detectionMethod"]?.GetValue<string>());
        Assert.Equal("HKLM", root["shared"]?["detection"]?["hive"]?.GetValue<string>());
        Assert.Equal("3.5.1", root["releases"]?[0]?["version"]?.GetValue<string>());
        Assert.Equal("2024-06-01T12:00:00Z", root["releases"]?[0]?["publishedAt"]?.GetValue<string>());
        JsonObject firstRelease = root["releases"]![0]!.AsObject();
        Assert.False(firstRelease.ContainsKey("checksum"));
        Assert.False(firstRelease.ContainsKey("disabled"));
    }

    private static Octokit.Release CreateCombinedArchitectureRelease() =>
        GitHubReleaseFactory.Create(
            "setup-v3.5.1",
            "DsHidMini Driver v3.5.1",
            "<!-- hidden metadata -->\nVisible notes",
            ("Nefarius_DsHidMini_Drivers_x64_arm64_v3.5.1.msi", "https://example.test/DsHidMini.msi", 18087936));

    private static async Task<UpdateResponse> ReadManifest(HttpResponseMessage response)
    {
        UpdateResponse? manifest = await response.Content.ReadFromJsonAsync<UpdateResponse>(SerializerOptions);
        Assert.NotNull(manifest);
        return manifest;
    }

    private static JsonSerializerOptions CreateSerializerOptions()
    {
        JsonSerializerOptions options = new()
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            PropertyNameCaseInsensitive = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
            PreferredObjectCreationHandling = JsonObjectCreationHandling.Populate
        };
        options.Converters.Add(new DateTimeOffsetConverter());
        options.Converters.Add(new JsonStringEnumConverter());
        return options;
    }
}
