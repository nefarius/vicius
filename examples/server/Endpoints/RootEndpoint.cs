using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints;

internal sealed class RootEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("/");
        AllowAnonymous();
    }

    public override Task HandleAsync(CancellationToken ct)
    {
        return SendRedirectAsync("https://docs.nefarius.at/projects/Vicius/", allowRemoteRedirects: true);
    }
}