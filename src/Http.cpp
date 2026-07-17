#include "pch.h"
#include "Http.h"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#pragma comment(lib, "winhttp.lib")


namespace
{
    // Convert a wide WinHTTP proxy string "host:port" into a libcurl proxy URL.
    // WinHTTP may return a semicolon-separated list; we take the first entry.
    std::string WinHttpProxyToUrl(const std::wstring& proxyList)
    {
        if (proxyList.empty()) return {};

        // Take the first entry before any ';'
        const auto sep = proxyList.find(L';');
        std::wstring first = (sep != std::wstring::npos) ? proxyList.substr(0, sep) : proxyList;

        // Trim whitespace
        while (!first.empty() && iswspace(first.front())) first.erase(first.begin());
        while (!first.empty() && iswspace(first.back()))  first.pop_back();
        if (first.empty()) return {};

        // Convert to narrow
        const int len = WideCharToMultiByte(CP_UTF8, 0, first.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string narrow(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, first.c_str(), -1, narrow.data(), len, nullptr, nullptr);

        // Prepend http:// if there's no scheme already
        if (narrow.find("://") == std::string::npos)
            narrow = "http://" + narrow;

        return narrow;
    }
}

namespace web
{
    std::optional<std::string> DetectSystemProxy(const std::string& targetUrl)
    {
        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieConfig{};
        if (!WinHttpGetIEProxyConfigForCurrentUser(&ieConfig))
        {
            // API failure — cannot determine proxy; fall through to direct.
            return std::nullopt;
        }

        // Guard: free the IE config strings on scope exit.
        auto freeIeConfig = [&]
        {
            if (ieConfig.lpszProxy)           GlobalFree(ieConfig.lpszProxy);
            if (ieConfig.lpszProxyBypass)     GlobalFree(ieConfig.lpszProxyBypass);
            if (ieConfig.lpszAutoConfigUrl)   GlobalFree(ieConfig.lpszAutoConfigUrl);
        };

        // Case 1: Static proxy configured directly.
        if (!ieConfig.fAutoDetect && ieConfig.lpszProxy && ieConfig.lpszAutoConfigUrl == nullptr)
        {
            std::string proxy = WinHttpProxyToUrl(ieConfig.lpszProxy);
            freeIeConfig();
            return proxy.empty() ? std::nullopt : std::optional<std::string>(proxy);
        }

        // Case 2: PAC script URL or WPAD auto-detect — must evaluate the PAC.
        const bool hasPacUrl   = (ieConfig.lpszAutoConfigUrl != nullptr);
        const bool autoDetect  = (ieConfig.fAutoDetect != FALSE);

        if (!hasPacUrl && !autoDetect)
        {
            freeIeConfig();
            return std::nullopt; // explicitly configured as direct
        }

        // Convert target URL to wide for WinHTTP APIs.
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, targetUrl.c_str(), -1, nullptr, 0);
        std::wstring wideTarget(wlen > 0 ? wlen - 1 : 0, L'\0');
        if (wlen > 0) MultiByteToWideChar(CP_UTF8, 0, targetUrl.c_str(), -1, wideTarget.data(), wlen);

        HINTERNET hSession = WinHttpOpen(
            L"Vicius/ProbeProxy",
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession)
        {
            freeIeConfig();
            return std::nullopt;
        }

        WINHTTP_AUTOPROXY_OPTIONS autoProxyOpts{};
        autoProxyOpts.dwFlags = 0;

        if (hasPacUrl)
        {
            autoProxyOpts.dwFlags            |= WINHTTP_AUTOPROXY_CONFIG_URL;
            autoProxyOpts.lpszAutoConfigUrl   = ieConfig.lpszAutoConfigUrl;
        }
        if (autoDetect)
        {
            autoProxyOpts.dwFlags            |= WINHTTP_AUTOPROXY_AUTO_DETECT;
            autoProxyOpts.dwAutoDetectFlags   = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
        }
        autoProxyOpts.fAutoLogonIfChallenged = TRUE;

        WINHTTP_PROXY_INFO proxyInfo{};
        const BOOL ok = WinHttpGetProxyForUrl(hSession, wideTarget.c_str(), &autoProxyOpts, &proxyInfo);

        WinHttpCloseHandle(hSession);
        freeIeConfig();

        if (!ok) return std::nullopt;

        std::optional<std::string> result;
        if (proxyInfo.lpszProxy && proxyInfo.dwAccessType != WINHTTP_ACCESS_TYPE_NO_PROXY)
        {
            std::string proxy = WinHttpProxyToUrl(proxyInfo.lpszProxy);
            if (!proxy.empty()) result = proxy;
        }

        if (proxyInfo.lpszProxy)       GlobalFree(proxyInfo.lpszProxy);
        if (proxyInfo.lpszProxyBypass) GlobalFree(proxyInfo.lpszProxyBypass);

        return result;
    }

    FailureKind ClassifyTransportError(const std::string& msg)
    {
        // All comparisons are case-insensitive so we lower-case once.
        std::string lo;
        lo.reserve(msg.size());
        for (unsigned char c : msg)
            lo.push_back(static_cast<char>(std::tolower(c)));

        // DNS resolution failure (curl error 6: "Couldn't resolve host …")
        if (lo.find("resolve host") != std::string::npos ||
            lo.find("couldn't resolve") != std::string::npos ||
            lo.find("could not resolve") != std::string::npos ||
            lo.find("name resolution") != std::string::npos)
        {
            return FailureKind::DnsFailure;
        }

        // TLS / SSL failure (curl errors 35, 51, 58, 59, 60, 77, 80, 82, 83…)
        if (lo.find("ssl") != std::string::npos ||
            lo.find("tls") != std::string::npos ||
            lo.find("certificate") != std::string::npos ||
            lo.find("handshake") != std::string::npos ||
            lo.find("peer cert") != std::string::npos)
        {
            return FailureKind::TlsError;
        }

        // Timeout / stall (curl errors 28, 29)
        if (lo.find("timeout") != std::string::npos ||
            lo.find("timed out") != std::string::npos ||
            lo.find("operation too slow") != std::string::npos)
        {
            return FailureKind::Timeout;
        }

        // Connection refused / unreachable (curl errors 7, 9)
        if (lo.find("connection refused") != std::string::npos ||
            lo.find("couldn't connect") != std::string::npos ||
            lo.find("could not connect") != std::string::npos ||
            lo.find("failed to connect") != std::string::npos ||
            lo.find("network unreachable") != std::string::npos ||
            lo.find("no route to host") != std::string::npos)
        {
            return FailureKind::ConnectionRefused;
        }

        return FailureKind::Other;
    }

    void RestrictRedirectProtocols(CURL* handle)
    {
        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    }

    std::expected<HttpResult, std::string> HttpGet(const std::string& url, const HttpGetOptions& opts)
    {
        HttpResult result{};

        // ----------------------------------------------------------------
        // Header + status-line parsing
        // ----------------------------------------------------------------
        auto headerCallback = [&](char* buffer, size_t size, size_t nitems) -> size_t
        {
            const size_t bytes = size * nitems;
            if (buffer == nullptr || bytes == 0) return bytes;

            std::string line(buffer, buffer + bytes);

            // Status line, e.g. "HTTP/1.1 200 OK" or "HTTP/2 200".
            // On redirects curl fires the header callback for each response in the
            // chain, so reset the accumulated headers each time a new response begins
            // so that result.headers only reflects the final response.
            if (line.rfind("HTTP/", 0) == 0)
            {
                result.headers.clear();
                const auto firstSpace = line.find(' ');
                if (firstSpace != std::string::npos && (firstSpace + 4) <= line.size())
                {
                    try { result.httpCode = std::stol(line.substr(firstSpace + 1, 3)); }
                    catch (...) {}
                }
                return bytes;
            }

            const auto colon = line.find(':');
            if (colon == std::string::npos) return bytes;

            std::string key   = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            while (!key.empty()   && isSpace(static_cast<unsigned char>(key.back())))    key.pop_back();
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))     value.pop_back();
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))  value.pop_back();

            if (!key.empty()) result.headers[key] = value;

            return bytes;
        };

        // ----------------------------------------------------------------
        // Body collection
        // ----------------------------------------------------------------
        auto writeCallback = [&](char* ptr, size_t size, size_t nmemb) -> size_t
        {
            const size_t bytes = size * nmemb;
            if (ptr == nullptr || bytes == 0) return bytes;
            result.body.append(ptr, bytes);
            return bytes;
        };

        // ----------------------------------------------------------------
        // curlpp request
        // ----------------------------------------------------------------

        // CURLOPT_RESOLVE entries must outlive the perform() call.
        curl_slist* resolveList = nullptr;

        try
        {
            curlpp::Easy req;
            req.setOpt(curlpp::options::Url(url));
            if (!opts.userAgent.empty())
                req.setOpt(curlpp::options::UserAgent(opts.userAgent));
            req.setOpt(curlpp::options::FollowLocation(true));
            req.setOpt(curlpp::options::MaxRedirs(opts.maxRedirects));
            RestrictRedirectProtocols(req.getHandle());
            if (!opts.headers.empty())
                req.setOpt(curlpp::options::HttpHeader(opts.headers));
            req.setOpt(curlpp::options::ConnectTimeout(opts.connectTimeoutSecs));
            if (opts.timeoutSecs > 0)
            {
                req.setOpt(curlpp::options::LowSpeedLimit(1));
                req.setOpt(curlpp::options::LowSpeedTime(opts.timeoutSecs));
            }
            req.setOpt(curlpp::options::WriteFunction(writeCallback));
            req.setOpt(curlpp::options::HeaderFunction(headerCallback));

            // Proxy — explicit value overrides; empty string means direct (no proxy).
            if (opts.proxy.has_value())
            {
                req.setOpt(curlpp::options::Proxy(opts.proxy.value()));
            }

            // IP version preference
            if (opts.ipResolve != CURL_IPRESOLVE_WHATEVER)
            {
                req.setOpt(curlpp::options::IpResolve(opts.ipResolve));
            }

            // DNS-over-HTTPS — requires libcurl >= 7.62; raw setopt since curlpp
            // does not expose a typed wrapper for CURLOPT_DOH_URL.
            if (!opts.dohUrl.empty())
            {
                curl_easy_setopt(req.getHandle(), CURLOPT_DOH_URL, opts.dohUrl.c_str());
            }

            // Static host→IP pins (CURLOPT_RESOLVE)
            if (!opts.resolveHosts.empty())
            {
                for (const auto& entry : opts.resolveHosts)
                    resolveList = curl_slist_append(resolveList, entry.c_str());
                curl_easy_setopt(req.getHandle(), CURLOPT_RESOLVE, resolveList);
            }

            req.perform();
        }
        catch (const curlpp::RuntimeError& e)
        {
            if (resolveList) curl_slist_free_all(resolveList);
            return std::unexpected(std::string(e.what()));
        }
        catch (const curlpp::LogicError& e)
        {
            if (resolveList) curl_slist_free_all(resolveList);
            return std::unexpected(std::string(e.what()));
        }

        if (resolveList) curl_slist_free_all(resolveList);
        return result;
    }
}
