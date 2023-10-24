using System.Text.Json.Serialization;

using FastEndpoints;

using Nefarius.Utilities.AspNetCore;

WebApplicationBuilder bld = WebApplication.CreateBuilder().Setup();
bld.Services.AddFastEndpoints();

WebApplication app = bld.Build().Setup();
app.UseFastEndpoints(c =>
{
    c.Serializer.Options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
});
app.Run();