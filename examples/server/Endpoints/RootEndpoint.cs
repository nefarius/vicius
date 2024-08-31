﻿using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints;

internal sealed class RootEndpoint(IWebHostEnvironment environment) : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("/");
        AllowAnonymous();
        Description(b => b.ExcludeFromDescription());
    }

    public override Task HandleAsync(CancellationToken ct)
    {
        return environment.IsDevelopment()
            ? SendNotFoundAsync(ct)
            : SendRedirectAsync("https://docs.nefarius.at/projects/Vicius/", allowRemoteRedirects: true);
    }
}