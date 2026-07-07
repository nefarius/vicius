#pragma once

#include "../ADL.hpp"

namespace models
{
    /**
     * \brief How the updater should obtain an HTTP proxy.
     */
    enum class ProxyMode
    {
        /**
         * Auto-detect from the Windows/WinINET system proxy settings
         * (static proxy, PAC URL, or WPAD). This is the default.
         */
        System,
        /** Always connect directly — no proxy, even if one is configured in the OS. */
        None,
        /** Use the URL specified in NetworkConfig::proxyUrl. */
        Manual,
        Invalid = -1,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ProxyMode, {
        {ProxyMode::Invalid, nullptr},
        {ProxyMode::System,  magic_enum::enum_name(ProxyMode::System)},
        {ProxyMode::None,    magic_enum::enum_name(ProxyMode::None)},
        {ProxyMode::Manual,  magic_enum::enum_name(ProxyMode::Manual)},
    })

    /**
     * \brief Which IP address family libcurl should use for name resolution.
     */
    enum class IpFamily
    {
        /** Let libcurl choose (CURL_IPRESOLVE_WHATEVER). Default. */
        Any,
        /** Force IPv4 name resolution (CURL_IPRESOLVE_V4). */
        V4,
        /** Force IPv6 name resolution (CURL_IPRESOLVE_V6). */
        V6,
        Invalid = -1,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(IpFamily, {
        {IpFamily::Invalid, nullptr},
        {IpFamily::Any,     magic_enum::enum_name(IpFamily::Any)},
        {IpFamily::V4,      magic_enum::enum_name(IpFamily::V4)},
        {IpFamily::V6,      magic_enum::enum_name(IpFamily::V6)},
    })

    /**
     * \brief A static host→IP pin entry fed to libcurl via CURLOPT_RESOLVE.
     *
     * Forces the named host to resolve to the supplied IP address, bypassing
     * normal DNS entirely for that host. Useful when DNS is poisoned or blocked.
     */
    struct PinnedHost
    {
        /** Hostname to pin, e.g. "vicius.api.nefarius.systems". */
        std::string host;
        /** Port number, e.g. 443. */
        int port{443};
        /** IP address to use, e.g. "104.21.0.1". May be IPv4 or IPv6. */
        std::string address;

        /**
         * \brief Renders the entry in the format expected by CURLOPT_RESOLVE:
         * "host:port:address"
         */
        [[nodiscard]] std::string ToCurlResolveEntry() const
        {
            return std::format("{}:{}:{}", host, port, address);
        }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PinnedHost, host, port, address)

    /**
     * \brief Per-deployment network resilience configuration, read from the
     * sidecar JSON under \c instance.network.
     *
     * All fields are optional; sensible defaults are applied when absent.
     */
    struct NetworkConfig
    {
        /**
         * How to obtain the proxy to use.
         * Default: System (auto-detect from Windows/WinINET settings).
         */
        ProxyMode proxyMode{ProxyMode::System};

        /**
         * Explicit proxy URL used when proxyMode == Manual.
         * Format: "http[s]://[user:pass@]host:port"
         * e.g. "http://proxy.corp.example.com:8080"
         */
        std::string proxyUrl;

        /**
         * DNS-over-HTTPS resolver URL.
         * When non-empty, libcurl uses DoH for all name resolutions in this
         * session, bypassing the system resolver (useful against DNS censorship).
         * Example: "https://cloudflare-dns.com/dns-query"
         * Requires libcurl >= 7.62.
         */
        std::string dohUrl;

        /**
         * IP address family preference for DNS resolution.
         * Default: Any (libcurl chooses). Set to V4 or V6 to force a specific
         * family (e.g. V4 for environments where IPv6 is unreliable).
         */
        IpFamily ipFamily{IpFamily::Any};

        /**
         * Static host→IP pin entries.
         * For each entry, libcurl bypasses DNS for that host and connects
         * directly to the pinned IP.  Useful when DNS is blocked/poisoned but
         * the IP is reachable.  The SNI hostname in TLS is still taken from the
         * URL, so the TLS certificate must match the hostname.
         */
        std::vector<PinnedHost> pinnedHosts;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        NetworkConfig, proxyMode, proxyUrl, dohUrl, ipFamily, pinnedHosts)
}
