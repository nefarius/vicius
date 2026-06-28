#pragma once

#include <curl/curl.h>
#include <string>
#include <list>
#include <unordered_map>


namespace web
{
    /**
     * \brief Result of a completed HttpGet call.
     *
     * curlCode carries the transport-level outcome (CURLE_OK on success).
     * httpCode holds the last parsed HTTP status line value (0 if none was received).
     * error is human-readable and non-empty when curlCode != CURLE_OK.
     */
    struct HttpResult
    {
        CURLcode curlCode{CURLE_OK};
        long httpCode{0};
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        std::string error;
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
     * On transport failure curlCode is set to the relevant CURLcode and error
     * contains a human-readable description; httpCode may still be non-zero if
     * a status line was received before the transfer was aborted.
     */
    HttpResult HttpGet(const std::string& url, const HttpGetOptions& opts);
}
