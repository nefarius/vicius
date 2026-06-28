#include "pch.h"
#include "Http.h"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>


namespace web
{
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
        try
        {
            curlpp::Easy req;
            req.setOpt(curlpp::options::Url(url));
            if (!opts.userAgent.empty())
                req.setOpt(curlpp::options::UserAgent(opts.userAgent));
            req.setOpt(curlpp::options::FollowLocation(true));
            req.setOpt(curlpp::options::MaxRedirs(opts.maxRedirects));
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

            req.perform();
        }
        catch (const curlpp::RuntimeError& e)
        {
            return std::unexpected(std::string(e.what()));
        }
        catch (const curlpp::LogicError& e)
        {
            return std::unexpected(std::string(e.what()));
        }

        return result;
    }
}
