#pragma once

#include <curl/curl.h>
#include <string>
#include <list>
#include <unordered_map>
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
     *
     * A proxy field is intentionally left as a commented stub so that
     * adding proxy support (issue #146) requires a change in one place only.
     */
    struct HttpGetOptions
    {
        std::string userAgent;
        std::list<std::string> headers;
        long timeoutSecs{0};
        long connectTimeoutSecs{60};
        long maxRedirects{5};
        // future: std::optional<std::string> proxy;
    };

    /**
     * \brief Performs an in-memory HTTP GET using curlpp.
     *
     * Returns the response on success. Returns std::unexpected with a
     * human-readable message on transport failure (connection error, TLS
     * handshake failure, stall timeout, etc.). HTTP error status codes (4xx,
     * 5xx) are NOT transport failures — they are returned inside HttpResult so
     * the caller can apply its own retry / fallback logic.
     *
     * Timeout failures include the word "timeout" in the error string, which
     * callers can test to apply exponential back-off.
     */
    std::expected<HttpResult, std::string> HttpGet(const std::string& url, const HttpGetOptions& opts);
}
