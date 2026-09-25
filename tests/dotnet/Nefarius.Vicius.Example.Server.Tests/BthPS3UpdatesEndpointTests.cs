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

public sealed class BthPS3UpdatesEndpointTests : IClassFixture<ServerFactory>
{
    private const string Path = "/api/nefarius/BthPS3/updates.json";

    private static readonly JsonSerializerOptions SerializerOptions = CreateSerializerOptions();

    private readonly ServerFactory _factory;

    public BthPS3UpdatesEndpointTests(ServerFactory factory)
    {
        _factory = factory;
        _factory.GitHub.LatestRelease = CreateArchitectureRelease();
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
        Assert.Equal("https://example.test/BthPS3_x64.msi", Assert.Single(manifest.Releases).DownloadUrl);
        Assert.Equal(new Version(2, 17, 0), manifest.Releases[0].Version);
        Assert.Equal("Visible notes", manifest.Releases[0].Summary);
        Assert.Equal("BthPS3", manifest.Shared?.ProductName);
        RegistryValueConfig detection = Assert.IsType<RegistryValueConfig>(manifest.Shared?.Detection);
        Assert.Equal(RegistryHive.HKLM, detection.Hive);
        Assert.Equal(@"SOFTWARE\Nefarius Software Solutions e.U.\Nefarius BthPS3 Bluetooth Drivers", detection.Key);
        Assert.Equal([0, 3010], manifest.Releases[0].ExitCode?.SuccessCodes);
    }

    [Theory]
    [InlineData("x64", "https://example.test/BthPS3_x64.msi")]
    [InlineData("arm64", "https://example.test/BthPS3_arm64.msi")]
    [InlineData("x86", "https://example.test/BthPS3_x86.msi")]
    public async Task Explicit_architecture_header_selects_the_matching_asset(
        string architecture,
        string expectedUrl)
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);
        request.Headers.TryAddWithoutValidation("X-Vicius-OS-Architecture", architecture);

        HttpResponseMessage response = await client.SendAsync(request);
        UpdateResponse manifest = await ReadManifest(response);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal(expectedUrl, Assert.Single(manifest.Releases).DownloadUrl);
    }

    [Fact]
    public async Task Unknown_architecture_header_returns_404()
    {
        HttpClient client = _factory.CreateClient();
        using HttpRequestMessage request = new(HttpMethod.Get, Path);
        request.Headers.TryAddWithoutValidation("X-Vicius-OS-Architecture", "riscv64");

        HttpResponseMessage response = await client.SendAsync(request);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Minisig_without_signer_returns_404()
    {
        HttpClient client = _factory.CreateClient();

        HttpResponseMessage response = await client.GetAsync("/api/nefarius/BthPS3/updates.json.minisig");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Unknown_filename_returns_404()
    {
        HttpClient client = _factory.CreateClient();

        HttpResponseMessage response = await client.GetAsync("/api/nefarius/BthPS3/notes.txt");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Theory]
    [InlineData("setup-v2.17.0", "2.17.0")]
    [InlineData("setup-v2.6.174.0", "2.6.174.0")]
    [InlineData("setup-v3.0.0-r6", "3.0.0")]
    [InlineData("setup-v3.0.0+build.5", "3.0.0")]
    public async Task Release_tag_normalizes_to_a_numeric_manifest_version(string tagName, string expectedVersion)
    {
        HttpClient client = _factory.CreateClient();
        ClearCache();
        _factory.GitHub.LatestRelease = CreateArchitectureRelease(tagName);
        try
        {
            HttpResponseMessage response = await client.GetAsync(Path);
            string json = await response.Content.ReadAsStringAsync();
            UpdateResponse? manifest = JsonSerializer.Deserialize<UpdateResponse>(json, SerializerOptions);
            JsonObject root = JsonNode.Parse(json)!.AsObject();

            Assert.Equal(HttpStatusCode.OK, response.StatusCode);
            Assert.NotNull(manifest);
            Assert.Equal(Version.Parse(expectedVersion), manifest.Releases[0].Version);
            Assert.Equal(expectedVersion, root["releases"]?[0]?["version"]?.GetValue<string>());
        }
        finally
        {
            _factory.GitHub.LatestRelease = CreateArchitectureRelease();
            ClearCache();
        }
    }

    [Theory]
    [InlineData("setup-v0-r6")]
    [InlineData("v3.0.0")]
    [InlineData("setup-v3.0")]
    [InlineData("setup-v3.0.0-")]
    [InlineData("setup-v3.0.0-rc..1")]
    [InlineData("setup-v03.0.0")]
    public async Task Unsupported_release_tag_returns_404(string tagName)
    {
        HttpClient client = _factory.CreateClient();
        ClearCache();
        _factory.GitHub.LatestRelease = CreateArchitectureRelease(tagName);
        try
        {
            HttpResponseMessage response = await client.GetAsync(Path);

            Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
        }
        finally
        {
            _factory.GitHub.LatestRelease = CreateArchitectureRelease();
            ClearCache();
        }
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
            _factory.GitHub.LatestRelease = CreateArchitectureRelease();
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
        Assert.Equal("2.17.0", root["releases"]?[0]?["version"]?.GetValue<string>());
        Assert.Equal("2024-06-01T12:00:00Z", root["releases"]?[0]?["publishedAt"]?.GetValue<string>());
        JsonObject firstRelease = root["releases"]![0]!.AsObject();
        Assert.False(firstRelease.ContainsKey("checksum"));
        Assert.False(firstRelease.ContainsKey("disabled"));
    }

    private void ClearCache()
    {
        if (_factory.Services.GetRequiredService<IMemoryCache>() is MemoryCache memoryCache)
            memoryCache.Clear();
    }

    private static Octokit.Release CreateArchitectureRelease(string tagName = "setup-v2.17.0") =>
        GitHubReleaseFactory.Create(
            tagName,
            "BthPS3 2.17.0",
            "<!-- hidden metadata -->\nVisible notes",
            ("Nefarius_BthPS3_Drivers_x64_v2.17.0.msi", "https://example.test/BthPS3_x64.msi", 15163392),
            ("Nefarius_BthPS3_Drivers_arm64_v2.17.0.msi", "https://example.test/BthPS3_arm64.msi", 14000000),
            ("Nefarius_BthPS3_Drivers_x86_v2.17.0.msi", "https://example.test/BthPS3_x86.msi", 12000000));

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
