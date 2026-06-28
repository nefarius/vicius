#pragma once
//
// E2E test overrides for the signed-manifest (Ed25519) test binary.
//   * Same HTTP-loopback allowance and server URL override as the main variant.
//   * Additionally compiles in the ephemeral minisign public key so that
//     manifest signature verification is mandatory for this build.
//
// CI replaces %%PUBLIC_KEY%% with the second line of the generated e2e.pub file
// (the base64 key string starting with "RWS...") before building this variant.
// The key is generated fresh every run and NEVER committed to the repository.
//
// Do NOT include in production builds.
//
#define NV_FLAGS_ALLOW_HTTP_DOWNLOAD

#undef  NV_API_URL_TEMPLATE
#define NV_API_URL_TEMPLATE "http://localhost:5200/api/{}/updates.json"

#define NV_MANIFEST_PUBLIC_KEY "%%PUBLIC_KEY%%"
