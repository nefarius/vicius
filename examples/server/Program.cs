using System.Text.Json.Serialization;

using FastEndpoints;
using FastEndpoints.Swagger;

using Nefarius.Utilities.AspNetCore;
using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Example.Server.Services;

WebApplicationBuilder bld = WebApplication.CreateBuilder().Setup();
bld.Services.AddFastEndpoints().SwaggerDocument(o =>
{
    // you will not need this but it is nice to have a summary
    o.DocumentSettings = s =>
    {
        s.Title = "vīcĭus updater API";
        s.Version = "v1";
        o.AutoTagPathSegmentIndex = 0;
        o.TagDescriptions = t =>
        {
            t["Schemas"] = "Schema definitions";
            t["Examples"] = "Example update configurations";
            t["Production"] = "Real-world product update configurations";
        };
    };
});

bld.Services.AddMemoryCache();
bld.Services.AddSingleton<GitHubApiService>();

WebApplication app = bld.Build().Setup();

// alter serializer settings to be compatible with the client-expected schema
app.UseFastEndpoints(c =>
{
    // the client can handle missing fields that are optional, no need to transmit null values
    c.Serializer.Options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
    // we exchange timestamps as ISO 8601 string (UTC)
    c.Serializer.Options.Converters.Add(new DateTimeOffsetConverter());
    // we use the enum value names (strings) instead of numerical values
    c.Serializer.Options.Converters.Add(new JsonStringEnumConverter());
}).UseSwaggerGen();

app.Run();