using System.Text.Json.Serialization;

using FastEndpoints;

using Nefarius.Utilities.AspNetCore;
using Nefarius.Vicius.Example.Server.Converters;

WebApplicationBuilder bld = WebApplication.CreateBuilder().Setup();
bld.Services.AddFastEndpoints();

WebApplication app = bld.Build().Setup();
app.UseFastEndpoints(c =>
{
    c.Serializer.Options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
    c.Serializer.Options.Converters.Add(new CustomDateTimeOffsetConverter("yyyy-MM-ddThh:mm:ssZ"));
});
app.Run();