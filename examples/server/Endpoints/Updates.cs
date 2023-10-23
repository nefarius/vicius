using FastEndpoints;

namespace server.Endpoints
{
	public sealed class Updates : EndpointWithoutRequest
	{
		public override void Configure()
		{
			Get("/updates.json");
			AllowAnonymous();
		}

		public override async Task HandleAsync(CancellationToken ct)
		{


			await SendOkAsync("", ct);
		}
	}
}
