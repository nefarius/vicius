using System.Text.Json.Serialization;

using FastEndpoints;
using FastEndpoints.Swagger;

using Minisign;

using Nefarius.Utilities.AspNetCore;
using Nefarius.Vicius.Abstractions.Converters;
using Nefarius.Vicius.Example.Server.Services;

// ── One-off CLI modes for E2E tooling (no web server started) ────────────────
//
// e2e-keygen <outDir>
//   Generates a fresh minisign key pair using the password from E2E_MINISIGN_PASSWORD,
//   writes e2e.key / e2e.pub to <outDir>, and prints the base64 public key string
//   (the second line of the .pub file, i.e. the RW... token) to stdout.
//   CI reads this value and compiles it into the updater as NV_MANIFEST_PUBLIC_KEY.
//
// e2e-sign <manifestPath>
//   Signs <manifestPath> using the key at E2E_MINISIGN_SECKEY decrypted with
//   E2E_MINISIGN_PASSWORD.  Produces <manifestPath>.minisig beside the manifest.
//   Replaces the minisign CLI's "minisign -S" step for the static E2E scenarios.
//
if (args.Length >= 2 && args[0] == "e2e-keygen")
{
    string outDir = args[1];
    string password = Environment.GetEnvironmentVariable("E2E_MINISIGN_PASSWORD")
                      ?? throw new InvalidOperationException("E2E_MINISIGN_PASSWORD is not set.");

    Directory.CreateDirectory(outDir);
    Core.GenerateKeyPair(password, writeOutputFiles: true, outputFolder: outDir, keyPairFileName: "e2e");

    // The .pub file written by minisign-net has two lines:
    //   untrusted comment: ...
    //   <base64 pubkey>   ← this is the RW... token compiled into NV_MANIFEST_PUBLIC_KEY
    string pubKeyFile = Path.Combine(outDir, "e2e.pub");
    string pubKeyLine = File.ReadLines(pubKeyFile).Skip(1).First();
    Console.WriteLine(pubKeyLine);
    return 0;
}

if (args.Length >= 2 && args[0] == "e2e-sign")
{
    string manifestPath = args[1];
    string secKeyPath = Environment.GetEnvironmentVariable("E2E_MINISIGN_SECKEY")
                        ?? throw new InvalidOperationException("E2E_MINISIGN_SECKEY is not set.");
    string password = Environment.GetEnvironmentVariable("E2E_MINISIGN_PASSWORD")
                      ?? throw new InvalidOperationException("E2E_MINISIGN_PASSWORD is not set.");

    var key = Core.LoadPrivateKeyFromFile(secKeyPath, password);
    Core.SignHashed(manifestPath, key);
    Console.WriteLine($"Signed: {manifestPath}.minisig");
    return 0;
}

// ── Normal web-server startup ─────────────────────────────────────────────────

WebApplicationBuilder bld = WebApplication.CreateBuilder(args).Setup();
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
bld.Services.AddSingleton<MinisignManifestSigner>();

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
return 0;