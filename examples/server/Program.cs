using FastEndpoints;

WebApplicationBuilder bld = WebApplication.CreateBuilder();
bld.Services.AddFastEndpoints();

WebApplication app = bld.Build();
app.UseFastEndpoints();
app.Run();