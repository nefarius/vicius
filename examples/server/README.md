# vīcĭus єאค๓קɭє รєгשєг

You found the example backend implementation! 🎉

I also use this project in production so it can be considered stable.

## Run the demo locally

The example server ships with a pre-wired happy-path demo that exercises every
working UI surface of the updater wizard.  After a fresh clone, getting a full
end-to-end run takes two steps:

**1. Start the example server** (Visual Studio: open `examples.sln`, set
`server` as the startup project, press F5 — or from the command line):

```PowerShell
cd examples/server
dotnet run
# Listening on http://localhost:5200
```

**2. Start the updater in Debug** (Visual Studio: open `vīcĭus.sln`, set the
`vīcĭus` project as the startup project, select **Debug | x64**, press F5).

The project ships a committed `src/vīcĭus.vcxproj.user` that passes these
arguments automatically:

```text
--server-url http://localhost:5200/api/demo/Showcase/updates.json
--log-level debug
--log-to-file $(TargetDir)debug.log
```

The updater will:
- Detect the installed version as **0.0.1** (always outdated), so the wizard
  always appears.
- Render the rich Markdown changelog (headings, lists, image, scrollbars, link).
- Show the **Help** and **Remind me tomorrow** buttons.
- Download the ~50 MB Microsoft-signed .NET Desktop Runtime (progress bar).
- Verify the Authenticode signature (Required + Strict, publisher pinned to subject `.NET` / issuer `Microsoft Code Signing PCA 2011`).
- Accept exit codes **0** and **3010** as success.

A `debug.log` file is written next to `Updater.exe` for each run.

Demo endpoint source: [`Endpoints/DefaultDemoEndpoint.cs`](Endpoints/DefaultDemoEndpoint.cs)

---

## How to build

Tag it with your own registry, of course 😉

```PowerShell
docker build --push -t nefarius.azurecr.io/nefarius-vicius-server:latest .
```

---

## Dynamic manifest signing (E2E / minisign-net)

The server binary doubles as a one-shot signing tool for E2E workflows.
No external `minisign` CLI is needed — all signing is done via
[minisign-net](https://github.com/bitbeans/minisign-net) (NuGet package).

### One-off CLI modes

Both modes exit immediately without starting the web server.

```PowerShell
# Generate an ephemeral Ed25519 key pair. Reads E2E_MINISIGN_PASSWORD.
# Writes e2e.key / e2e.pub to <outDir> and prints the base64 public key
# (the RW... token) to stdout so it can be compiled into NV_MANIFEST_PUBLIC_KEY.
$env:E2E_MINISIGN_PASSWORD = 'your-password'
dotnet examples/server/bin/Release/net10.0/server.dll e2e-keygen <outDir>

# Sign a manifest file. Reads E2E_MINISIGN_SECKEY and E2E_MINISIGN_PASSWORD.
# Produces <manifestPath>.minisig beside the manifest.
$env:E2E_MINISIGN_SECKEY   = '<outDir>/e2e.key'
$env:E2E_MINISIGN_PASSWORD = 'your-password'
dotnet examples/server/bin/Release/net10.0/server.dll e2e-sign <manifestPath>
```

### Runtime dynamic signing endpoint

When the web server starts with `E2E_MINISIGN_SECKEY` and `E2E_MINISIGN_PASSWORD`
set, `MinisignManifestSigner` loads the private key and enables two additional E2E
routes under the `e2eSigDyn` manufacturer prefix:

| Route | Description |
|---|---|
| `GET api/e2eSigDyn/DynamicSignedManifest/updates.json` | Builds and serves the canonical manifest JSON |
| `GET api/e2eSigDyn/DynamicSignedManifest/updates.json.minisig` | Returns the Ed25519 signature over the canonical bytes |
| `GET api/e2eSigDyn/DynamicTamperedManifest/updates.json` | Serves a mutated (tampered) manifest body |
| `GET api/e2eSigDyn/DynamicTamperedManifest/updates.json.minisig` | Returns the signature over the *untampered* body — so client verification fails |

The signature format is minisign's prehashed "ED" mode (Ed25519 over BLAKE2b-512),
which is interoperable with the C++ client's `VerifyManifestSignature` implementation.

---

## Sources & 3rd party credits

- [NJsonSchema for .NET](https://github.com/RicoSuter/NJsonSchema)
- [FastEndpoints](https://fast-endpoints.com/)
- [minisign-net](https://github.com/bitbeans/minisign-net)
- [Nefarius.Utilities.AspNetCore](https://github.com/nefarius/Nefarius.Utilities.AspNetCore)
- [Nefarius.Vicius.Abstractions](../../abstractions/) (co-located in this repo)
