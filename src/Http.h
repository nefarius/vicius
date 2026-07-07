#pragma once

#include <curl/curl.h>
#include <string>
#include <list>
#include <unordered_map>
#include <optional>
#include <expected>


namespace web
{
    /**
     * \brief Payload returned by a successful HttpGet call.
     *
     * httpCode holds the last parsed HTTP status line value.
     * The caller is responsible for checking whether the status is acceptable
     * (e.g. 200 OK vs 404 Not Found); HttpGet only fails at the transport level.
     */
    struct HttpResult
    {
        long httpCode{0};
        std::string body;
        std::unordered_map<std::string, std::string> headers;
    };

    /**
     * \brief Options for HttpGet.
     */
    struct HttpGetOptions
    {
        std::string userAgent;
        std::list<std::string> headers;
        long timeoutSecs{0};
        long connectTimeoutSecs{60};
        long maxRedirects{5};

        /**
         * Proxy URL, e.g. "http://user:pass@proxy.corp:8080".
         * When empty the request goes direct (no proxy).
         */
        std::optional<std::string> proxy;

        /**
         * DNS-over-HTTPS resolver URL, e.g. "https://cloudflare-dns.com/dns-query".
         * When non-empty, curl uses DoH for name resolution of this request.
         * Requires libcurl >= 7.62.
         */
        std::string dohUrl;

        /**
         * Static host→IP pin entries fed to CURLOPT_RESOLVE.
         * Each entry must be in curl's format: "host:port:address[,address2]"
         * e.g. "vicius.api.nefarius.systems:443:104.21.0.1"
         */
        std::list<std::string> resolveHosts;

        /**
         * IP version preference for DNS resolution (CURLOPT_IPRESOLVE).
         * Accepted values: CURL_IPRESOLVE_WHATEVER (0), CURL_IPRESOLVE_V4 (1),
         * CURL_IPRESOLVE_V6 (2).
         */
        long ipResolve{CURL_IPRESOLVE_WHATEVER};
    };

    /**
     * \brief Coarse failure category for a transport error string returned by HttpGet.
     *
     * Used by callers to apply category-specific recovery (DoH retry on DnsFailure,
     * fast failover on ConnectionRefused/TlsError, timeout escalation on Timeout).
     */
    enum class FailureKind
    {
        None,
        DnsFailure,        ///< Could not resolve the host name
        ConnectionRefused, ///< Host reachable but refused the connection
        TlsError,          ///< TLS/SSL handshake or certificate failure
        Timeout,           ///< Connection or transfer stall timeout
        Other,             ///< Any other transport error
    };

    /**
     * \brief Maps a curlpp transport error string to a FailureKind.
     *
     * The error strings produced by libcurl are stable English prose sourced from
     * curl_easy_strerror(). We match on stable substrings that have not changed
     * across the libcurl versions in use.
     */
    FailureKind ClassifyTransportError(const std::string& curlMessage);

    /**
     * \brief Detects the Windows system/WinINET proxy for the given target URL.
     *
     * Reads the per-user IE/WinINET proxy settings and, when a PAC script or
     * WPAD auto-detect is configured, evaluates the PAC for the supplied URL.
     *
     * \param targetUrl The URL that will be fetched (used for PAC evaluation).
     * \return A proxy URL string (e.g. "http://proxy.corp:8080") when a proxy
     *         is configured for the target; std::nullopt when the connection
     *         should be direct (no proxy).
     */
    std::optional<std::string> DetectSystemProxy(const std::string& targetUrl);

    /**
     * \brief Performs an in-memory HTTP GET using curlpp.
     *
     * Returns the response on success. Returns std::unexpected with a
     * human-readable message on transport failure (connection error, TLS
     * handshake failure, stall timeout, etc.). HTTP error status codes (4xx,
     * 5xx) are NOT transport failures — they are returned inside HttpResult so
     * the caller can apply its own retry / fallback logic.
     */
    std::expected<HttpResult, std::string> HttpGet(const std::string& url, const HttpGetOptions& opts);
}
