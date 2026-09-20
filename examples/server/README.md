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

Pushing a tag of the form `server-vX.X.X` (for example `server-v1.2.3`)
publishes the image to Docker Hub as `containinger/vicius-server:X.X.X` and
`containinger/vicius-server:latest`. The workflow logs in as `containinger`
using the `DOCKERHUB_TOKEN` repository secret (a Docker Hub access token).

```PowerShell
docker pull containinger/vicius-server:latest
```

To build locally from the repository root:

```PowerShell
docker build -f examples/server/Dockerfile -t containinger/vicius-server:latest .
```

---

## Dynamic manifest signing (minisign-net)

The server signs manifests via
[minisign-net](https://github.com/bitbeans/minisign-net) (NuGet package).
No external `minisign` CLI is required.

`MinisignManifestSigner` loads two independent key pairs at startup:

1. `MINISIGN_SECKEY` / `MINISIGN_PASSWORD` — production product routes (BthPS3, DsHidMini) and the demo/Updater aliases
2. `E2E_MINISIGN_SECKEY` / `E2E_MINISIGN_PASSWORD` — E2E routes only

The E2E pair is never used to sign production or demo routes. A missing pair
disables only that scope. Existing unsigned clients keep working: they only
fetch `updates.json` and never look at the sidecar.

### Production: BthPS3

| Route | Description |
|---|---|
| `GET api/nefarius/BthPS3/updates.json` | Latest BthPS3 manifest (same JSON schema as before) |
| `GET api/nefarius/BthPS3/updates.json.minisig` | Ed25519 sidecar over those exact bytes; **404** if the signer is not configured |

Both routes honor `X-Vicius-OS-Architecture` (default `x64`) so each arch gets
a matching json+minisig pair. Old BthPS3 clients that know nothing about
signatures continue to consume `updates.json` unchanged.

### Production: DsHidMini

| Route | Description |
|---|---|
| `GET api/nefarius/DsHidMini/updates.json` | Latest DsHidMini manifest |
| `GET api/nefarius/DsHidMini/updates.json.minisig` | Ed25519 sidecar over those exact bytes; **404** if the signer is not configured |

DsHidMini ships a combined x64/ARM64 MSI. `X-Vicius-OS-Architecture` of `x64`
or `arm64` (default `x64`) selects that asset. `x86` and any other value
return **404**.

### Examples: demo / Updater

| Route | Description |
|---|---|
| `GET api/demo/Showcase/updates.json` | Default demo manifest (also used by local Debug runs) |
| `GET api/demo/Showcase/updates.json.minisig` | Ed25519 sidecar over those exact bytes; **404** if the signer is not configured |
| `GET api/Updater/updates.json` | Same snapshot as Showcase (plain Debug tenant path) |
| `GET api/Updater/updates.json.minisig` | Same sidecar as Showcase |
| `GET api/example/Demo/updates.json` | Same snapshot as Showcase (downloadable `example_Demo_Updater` tenant path) |
| `GET api/example/Demo/updates.json.minisig` | Same sidecar as Showcase |

Serialized JSON and the sidecar are built once per snapshot (per architecture
for BthPS3 and DsHidMini; once for the demo aliases) and cached in memory for
one hour (including Development), so both routes always serve the same bytes.
Successful responses also send `Cache-Control: public, max-age=3600` for any
reverse proxy.

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

When `E2E_MINISIGN_SECKEY` and `E2E_MINISIGN_PASSWORD` are set, two additional
E2E routes are available under the `e2eSigDyn` manufacturer prefix:

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
