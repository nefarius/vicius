using FastEndpoints;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     E2E scenario: manifest URL returns a captive-portal/censorship block page (HTTP 200, text/html).
///     The updater must detect the non-JSON body and fall back to the next candidate URL.
///     Only active when <c>VICIUS_E2E=1</c>.
/// </summary>
internal sealed class E2EBlockPageEndpoint : EndpointWithoutRequest
{
    public override void Configure()
    {
        Get("api/e2e/BlockPage/updates.json");
        AllowAnonymous();
        Options(x => x.WithTags("E2E"));
    }

    public override async Task HandleAsync(CancellationToken ct)
    {
        if (!E2EGuard.IsEnabled)
        {
            await Send.NotFoundAsync(ct);
            return;
        }

        const string body = """
            <!DOCTYPE html>
            <html>
            <head><title>Access Denied</title></head>
            <body>
            <h1>Access Denied</h1>
            <p>This content has been blocked by your network administrator.</p>
            </body>
            </html>
            """;

        HttpContext.Response.ContentType = "text/html; charset=utf-8";
        HttpContext.Response.StatusCode = 200;
        await HttpContext.Response.WriteAsync(body, ct);
    }
}
