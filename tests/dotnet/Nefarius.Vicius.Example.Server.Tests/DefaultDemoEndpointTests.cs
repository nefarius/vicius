using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;

using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.DependencyInjection;

using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Tests;

public sealed class DefaultDemoEndpointTests : IClassFixture<ServerFactory>
{
    private static readonly JsonSerializerOptions SerializerOptions = CreateSerializerOptions();

    private readonly ServerFactory _factory;
    private readonly HttpClient _client;

    public DefaultDemoEndpointTests(ServerFactory factory)
    {
        _factory = factory;
        _client = factory.CreateClient();
    }

    [Theory]
    [InlineData("/api/demo/Showcase/updates.json")]
    [InlineData("/api/Updater/updates.json")]
    [InlineData("/api/example/Demo/updates.json")]
    public async Task Manifest_aliases_return_the_demo_product_and_cache_headers(string path)
    {
        HttpResponseMessage response = await _client.GetAsync(path);
        UpdateResponse? manifest = await response.Content.ReadFromJsonAsync<UpdateResponse>(SerializerOptions);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.True(response.Headers.CacheControl?.Public);
        Assert.Equal(TimeSpan.FromHours(1), response.Headers.CacheControl?.MaxAge);
        Assert.NotNull(manifest);
        Assert.Equal("Vicius Demo Product", manifest.Shared?.ProductName);
        Assert.Equal(new Version(99, 0, 0), Assert.Single(manifest.Releases).Version);
    }

    [Fact]
    public async Task Showcase_updater_and_example_aliases_serve_identical_cached_bytes()
    {
        HttpResponseMessage showcase = await _client.GetAsync("/api/demo/Showcase/updates.json");
        HttpResponseMessage updater = await _client.GetAsync("/api/Updater/updates.json");
        HttpResponseMessage example = await _client.GetAsync("/api/example/Demo/updates.json");

        byte[] showcaseBytes = await showcase.Content.ReadAsByteArrayAsync();
        byte[] updaterBytes = await updater.Content.ReadAsByteArrayAsync();
        byte[] exampleBytes = await example.Content.ReadAsByteArrayAsync();

        Assert.Equal(HttpStatusCode.OK, showcase.StatusCode);
        Assert.Equal(HttpStatusCode.OK, updater.StatusCode);
        Assert.Equal(HttpStatusCode.OK, example.StatusCode);
        Assert.Equal(showcaseBytes, updaterBytes);
        Assert.Equal(showcaseBytes, exampleBytes);
    }

    [Fact]
    public async Task Concurrent_cache_misses_share_one_snapshot()
    {
        if (_factory.Services.GetRequiredService<IMemoryCache>() is MemoryCache memoryCache)
            memoryCache.Clear();

        Task<HttpResponseMessage> firstTask = _client.GetAsync("/api/demo/Showcase/updates.json");
        Task<HttpResponseMessage> secondTask = _client.GetAsync("/api/Updater/updates.json");
        HttpResponseMessage[] responses = await Task.WhenAll(firstTask, secondTask);

        byte[] firstBytes = await responses[0].Content.ReadAsByteArrayAsync();
        byte[] secondBytes = await responses[1].Content.ReadAsByteArrayAsync();

        Assert.Equal(HttpStatusCode.OK, responses[0].StatusCode);
        Assert.Equal(HttpStatusCode.OK, responses[1].StatusCode);
        Assert.Equal(firstBytes, secondBytes);
    }

    [Theory]
    [InlineData("/api/demo/Showcase/updates.json.minisig")]
    [InlineData("/api/Updater/updates.json.minisig")]
    [InlineData("/api/example/Demo/updates.json.minisig")]
    public async Task Minisig_without_signer_returns_404(string path)
    {
        HttpResponseMessage response = await _client.GetAsync(path);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Theory]
    [InlineData("/api/demo/Showcase/notes.txt")]
    [InlineData("/api/Updater/notes.txt")]
    [InlineData("/api/example/Demo/notes.txt")]
    public async Task Unknown_filename_returns_404(string path)
    {
        HttpResponseMessage response = await _client.GetAsync(path);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
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
