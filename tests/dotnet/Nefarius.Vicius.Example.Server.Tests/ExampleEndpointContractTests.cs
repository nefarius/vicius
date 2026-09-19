using System.Net;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Example.Server.Tests;

public sealed class ExampleEndpointContractTests : IClassFixture<ServerFactory>
{
    private static readonly JsonSerializerOptions SerializerOptions = CreateSerializerOptions();

    private readonly HttpClient _client;

    public ExampleEndpointContractTests(ServerFactory factory)
    {
        _client = factory.CreateClient();
    }

    [Theory]
    [InlineData("/api/contoso/Minimal/updates.json", ".NET Runtime")]
    [InlineData("/api/demo/Showcase/updates.json", "Vicius Demo Product")]
    [InlineData("/api/Updater/updates.json", "Vicius Demo Product")]
    public async Task Example_update_endpoints_return_deserializable_manifests(
        string path,
        string expectedProduct)
    {
        HttpResponseMessage response = await _client.GetAsync(path);
        string body = await response.Content.ReadAsStringAsync();
        JsonObject root = JsonNode.Parse(body)!.AsObject();
        UpdateResponse? manifest = JsonSerializer.Deserialize<UpdateResponse>(body, SerializerOptions);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.NotNull(manifest);
        Assert.Equal(expectedProduct, manifest.Shared?.ProductName);
        Assert.NotEmpty(manifest.Releases);
        Assert.False(string.IsNullOrWhiteSpace(manifest.Releases[0].DownloadUrl));
        Assert.Equal(JsonValueKind.String, root["releases"]?[0]?["version"]?.GetValueKind());
        Assert.Equal(JsonValueKind.String, root["releases"]?[0]?["publishedAt"]?.GetValueKind());
        Assert.EndsWith("Z", root["releases"]?[0]?["publishedAt"]?.GetValue<string>());
        Assert.Null(root["releases"]?[0]?["disabled"]);
    }

    [Fact]
    public async Task Schema_endpoint_returns_update_response_schema()
    {
        HttpResponseMessage response = await _client.GetAsync("/api/vicius/master/schema.json");
        JsonObject schema = JsonNode.Parse(await response.Content.ReadAsStringAsync())!.AsObject();

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal("object", schema["type"]?.GetValue<string>());
        Assert.True(schema["properties"]?["releases"] is not null);
        Assert.True(schema["properties"]?["shared"] is not null);
        Assert.True(schema["properties"]?["instance"] is not null);
    }

    [Fact]
    public async Task E2E_routes_are_unavailable_when_the_guard_is_disabled()
    {
        HttpResponseMessage response = await _client.GetAsync("/api/e2e/HappyExe/updates.json");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Production_route_without_a_github_release_does_not_call_the_network()
    {
        HttpResponseMessage response = await _client.GetAsync("/api/nefarius/HidHide/updates.json");

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
