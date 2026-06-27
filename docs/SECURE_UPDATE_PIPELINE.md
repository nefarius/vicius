# Secure Update Pipeline

This document describes the five-layer defense-in-depth model implemented in vicius and how to configure each layer.

---

## Architecture overview

```
Fetch updates.json over TLS
         │
         ▼
 [Layer 3] Ed25519 manifest signature valid? (NV_MANIFEST_PUBLIC_KEY compiled in)
         │ no  → abort
         │ yes
         ▼
 Parse and trust manifest
         │
         ▼
 [Layer 5] HTTPS enforced on all downloadUrl / latestUrl
         │ non-HTTPS → abort
         │
         ▼
 Download setup over HTTPS
         │
         ▼
 [Layer 1] Setup checksum matches manifest? (release.checksum)
         │ mismatch → abort
         │
         ▼
 [Layer 2] Authenticode chain valid AND publisher pinned?
         │ no  → abort (in Required / Strict mode)
         │ yes
         ▼
 Run setup / extract zip
```

Each layer is independently opt-in so existing deployments keep working while
tenants migrate to stricter settings.

---

## Layer 1 — Setup checksum (always-on when provided)

Add a `checksum` block to each release entry:

```json
{
  "releases": [
    {
      "checksum": {
        "checksum": "<sha256-hex>",
        "checksumAlg": "SHA256"
      }
    }
  ]
}
```

Supported algorithms: `MD5`, `SHA1`, `SHA256`.

When a checksum is present it **must** match before the setup is executed.
When absent:
- **Relaxed mode** (default): allowed, a warning is logged.
- **Strict mode** (`--strict-verification` CLI): the setup is rejected.

---

## Layer 2 — Authenticode publisher pinning

### Global settings (`shared` block)

```json
{
  "shared": {
    "signatureVerificationMode": "WhenPresent",
    "signaturePolicy": "Relaxed",
    "signatureStrategy": "FromUpdaterBinary"
  }
}
```

| Field | Values | Default |
|---|---|---|
| `signatureVerificationMode` | `Disabled`, `WhenPresent`, `Required` | `WhenPresent` |
| `signaturePolicy` | `Relaxed` (chain only), `Strict` (chain + pin) | `Relaxed` |
| `signatureStrategy` | `FromUpdaterBinary`, `FromConfiguration` | `FromUpdaterBinary` |

### FromUpdaterBinary (default, renewal-safe)

The setup's signing certificate subject name is compared against the updater exe's
own subject name.  **Only `subjectName` is compared** — this field is stable across
certificate renewals (tied to your legal entity).

### FromConfiguration (explicit pin)

Supply an explicit pin in the `signatureConfig` block.  Because this travels inside
the signed manifest (Layer 3), rotating it on cert renewal is safe:

```json
{
  "shared": {
    "signatureVerificationMode": "Required",
    "signaturePolicy": "Strict",
    "signatureStrategy": "FromConfiguration",
    "signatureConfig": {
      "subjectName": "My Company, Inc.",
      "issuerName":  "DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1"
    }
  }
}
```

Per-release overrides are also supported (add `signature`, `signaturePolicy`,
`signatureStrategy` inside the release object).

### Certificate renewal runbook

> **Non-negotiable anti-brick guarantee:** an updater that statically pins serial /
> thumbprint will mass-lockout all deployed clients when the cert renews.

Safe renewal procedure for Strict/FromConfiguration:

1. Sign the new manifest (with cert B's identity) **while cert A is still valid**.
2. Publish an updated `signatureConfig` with cert B's `subjectName` (or add
   `serialNumber`/`thumbprint` if you want extra-strict pinning).
3. Sign and publish the manifest with the Ed25519 key (same key — no redeploy).
4. Deployed clients accept cert B through the manifest before you stop using cert A.

If you use **`FromUpdaterBinary`** with `subjectName` comparison only, no action is
needed on renewal — the organization name doesn't change.

---

## Layer 3 — Ed25519 / minisign manifest signature

### Generate a keypair

```sh
minisign -G
# writes ~/.minisign/minisign.key  and  ~/.minisign/minisign.pub
```

### Compile the public key into the updater

Open `src/CustomizeMe.h` and uncomment + fill in the second line of your `.pub` file:

```cpp
#define NV_MANIFEST_PUBLIC_KEY  "RWSxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

When `NV_MANIFEST_PUBLIC_KEY` is defined, manifest verification is **mandatory**.

### Sign the manifest on every publish

```sh
minisign -S -s ~/.minisign/minisign.key -m updates.json
# produces updates.json.minisig alongside updates.json
```

Serve both files from the same URL base.  The updater automatically fetches
`<manifestUrl>.minisig` and verifies it before parsing the JSON.

### Rollback / downgrade protection

Add a monotonically increasing `manifestVersion` integer to the root of the manifest:

```json
{
  "manifestVersion": 42,
  ...
}
```

Deployed clients persist the highest version seen in the registry and reject
manifests with a lower value (downgrade / replay attack).

---

## Layer 4 — Self-updater hardening

The self-updater DLL (`dll/dll.cpp`) now:

1. Downloads the new updater binary to a **temporary file** (not the final path).
2. Verifies the **checksum** (`--checksum` / `--checksum-alg` CLI args, forwarded
   from the signed manifest via `instance.latestChecksum`).
3. Verifies the **Authenticode signature** using WinVerifyTrust (revocation checked,
   no `WTD_LIFETIME_SIGNING_FLAG` — timestamped expired certs remain valid).
4. Only on success: atomically swaps the temp file into the final path.
5. On any failure: restores the original binary from the hidden backup.

To supply a checksum for the self-updater binary, add `latestChecksum` to the
`instance` block:

```json
{
  "instance": {
    "latestVersion": "2.0.0",
    "latestUrl": "https://updates.example.com/Updater.exe",
    "latestChecksum": {
      "checksum": "<sha256-hex-of-Updater.exe>",
      "checksumAlg": "SHA256"
    }
  }
}
```

---

## Layer 5 — URL scheme enforcement

`downloadUrl` and `latestUrl` must use `https://`.  Plain `http://`, `file://`,
and any other scheme are rejected.  In debug builds (`!NDEBUG`), HTTP to localhost
(`127.0.0.1`, `[::1]`) is permitted for local test servers.

---

## Strict mode CLI flag

Pass `--strict-verification` to the updater binary to enable strict mode globally:

- Checksum absent → reject (instead of warn-and-allow).
- Authenticode mode upgraded from `WhenPresent` to `Required`.
- Comparison policy upgraded from `Relaxed` to `Strict`.

---

## Example manifest

See `examples/configs/example_secure_updates.json` for a fully-annotated example
that demonstrates all layers.
