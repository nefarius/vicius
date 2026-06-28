#pragma once
//
// E2E test overrides for the main (unsigned) test binary.
//   * Allows HTTP downloads from loopback addresses (localhost / 127.0.0.1 / ::1)
//     so the updater can talk to the local test server in Release builds.
//   * Points every release build at the local test server rather than the
//     production endpoint.
//
// Do NOT include in production builds.
//
#define NV_FLAGS_ALLOW_HTTP_DOWNLOAD

// CustomizeMe.h already defined NV_API_URL_TEMPLATE unconditionally before this
// post-header is included, so we must undef first.
#undef  NV_API_URL_TEMPLATE
#define NV_API_URL_TEMPLATE "http://localhost:5200/api/{}/updates.json"
