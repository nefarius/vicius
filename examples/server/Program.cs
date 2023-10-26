using System.Text.Json.Serialization;

using FastEndpoints;
using FastEndpoints.Swagger;

using Nefarius.Utilities.AspNetCore;
using Nefarius.Vicius.Example.Server.Converters;

WebApplicationBuilder bld = WebApplication.CreateBuilder().Setup();
bld.Services.AddFastEndpoints().SwaggerDocument(o =>
{
    // you will not need this but it is nice to have a summary
    o.DocumentSettings = s =>
    {
        s.Title = "vīcĭus updater API";
        s.Version = "v1";
    };
});

WebApplication app = bld.Build().Setup();

// alter serializer settings to be compatible with the client-expected schema
app.UseFastEndpoints(c =>
{
    // the client can handle missing fields that are optional, no need to transmit null values
    c.Serializer.Options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
    // we exchange timestamps as ISO 8601 string (UTC)
    c.Serializer.Options.Converters.Add(new CustomDateTimeOffsetConverter("yyyy-MM-ddThh:mm:ssZ"));
    // we use the enum value names (strings) instead of numerical values
    c.Serializer.Options.Converters.Add(new JsonStringEnumConverter());
}).UseSwaggerGen();

app.Run();