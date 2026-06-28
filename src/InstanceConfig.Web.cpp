#include "pch.h"
#include "Http.h"
#include "Util.h"
#include "InstanceConfig.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>


std::list<std::string> models::InstanceConfig::BuildCommonHeaders() const
{
    std::list<std::string> lines;

#if !defined(NV_FLAGS_NO_VENDOR_HEADERS)
    lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-Manufacturer: {}", manufacturer));
    lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-Product: {}", product));
    lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-Version: {}", appVersion.str()));
    if (!channel.empty())
        lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-Channel: {}", channel));

#if defined(_M_AMD64)
    lines.push_back("X-" NV_HTTP_HEADERS_NAME "-Process-Architecture: x64");
#elif defined(_M_ARM64)
    lines.push_back("X-" NV_HTTP_HEADERS_NAME "-Process-Architecture: arm64");
#else
    lines.push_back("X-" NV_HTTP_HEADERS_NAME "-Process-Architecture: x86");
#endif

    const SYSTEM_INFO si = nefarius::winapi::SafeGetNativeSystemInfo();
    std::string arch;
    switch (si.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64";   break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86";   break;
        default:                           arch = "<unknown>"; break;
    }
    lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-OS-Architecture: {}", arch));

    for (const auto& [fst, snd] : additionalHeaders)
        lines.push_back(std::format("X-" NV_HTTP_HEADERS_NAME "-{}: {}", fst, snd));
#endif

    return lines;
}

std::expected<int, std::string> models::InstanceConfig::DownloadRelease(curl_progress_callback progressFn, const int releaseIndex)
{
    UNREFERENCED_PARAMETER(releaseIndex);
    abortDownloadRequested.store(false, std::memory_order_relaxed);

    std::string downloadLocation;

    if (!merged.downloadLocation.input.empty())
    {
        const auto& rendered = RenderInjaTemplate(merged.downloadLocation.input, merged.downloadLocation.data);

        if (nefarius::winapi::fs::DirectoryCreate(rendered))
        {
            downloadLocation = rendered;
        }
        else
        {
            spdlog::error("Failed to create download location {}, defaulting to TEMP path", rendered);

            if (const auto tmp = winapi::GetUserTemporaryDirectory(); tmp)
                downloadLocation = *tmp;
        }
    }
    else
    {
        if (const auto tmp = winapi::GetUserTemporaryDirectory(); tmp)
            downloadLocation = *tmp;
    }

    // if we're still empty, the previous methods all failed
    if (downloadLocation.empty())
    {
        // fallback to writable location for all users
        const auto programDataDir = winapi::GetProgramDataPath();
        if (!programDataDir)
        {
            spdlog::error("Failed to get %ProgramData% directory: {}", programDataDir.error());
            return std::unexpected(programDataDir.error());
        }

        // build new absolute path, e.g. "C:\ProgramData\nefarius\HidHide\downloads" or "C:\ProgramData\Updater\downloads"
        const std::filesystem::path subDir = manufacturer.empty() ? appFilename : std::filesystem::path(manufacturer) / product;
        const std::filesystem::path targetDirectory = std::filesystem::path(*programDataDir) / subDir / "downloads";

        // ensure this location can be created
        if (!nefarius::winapi::fs::DirectoryCreate(targetDirectory.string()))
        {
            spdlog::error("Failed to create fallback download location {}, error: {:#x}", targetDirectory,
                          GetLastError());
            return std::unexpected(std::format("Failed to create download directory '{}': {}", targetDirectory.string(),
                                               winapi::GetLastErrorStdStr()));
        }

        downloadLocation = targetDirectory.string();
    }

    std::string tempFile{};

    auto& release = GetSelectedRelease();

    // Reuse existing temp file if present (allows resuming across user retries)
    if (release.localTempFilePath.empty())
    {
        const auto newTempFile = winapi::GetNewTemporaryFile(downloadLocation);
        if (!newTempFile)
        {
            spdlog::error("Failed to get temporary file name: {}", newTempFile.error());
            return std::unexpected(newTempFile.error());
        }
        tempFile = *newTempFile;

        spdlog::debug("tempFile = {}", tempFile);
        release.localTempFilePath = tempFile;
    }
    else
    {
        spdlog::debug("Using existing temp file {}", release.localTempFilePath);
    }

    std::ofstream outStream{};
    const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
    outStream.exceptions(exceptionMask);

    int retryCount = MAX_RETRY_COUNT;
    long currentTimeoutSecs = MAX_TIMEOUT_SECS;
    constexpr long kMaxTimeoutSecs = MAX_TIMEOUT_SECS_TOTAL;
    std::string lastFailureDetails{};
    std::optional<std::filesystem::path> cachedAttachmentName{};

    auto tryGetHeader = [](const auto& headers, const std::string& key) -> std::string
    {
        if (const auto it = headers.find(key); it != headers.end())
        {
            return it->second;
        }

        // Some stacks may lowercase header names.
        std::string lowerKey = key;
        for (auto& ch : lowerKey)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (const auto it2 = headers.find(lowerKey); it2 != headers.end())
        {
            return it2->second;
        }

        return {};
    };

    auto tryParseTotalSizeFromContentRange = [](const std::string& contentRange) -> std::optional<uint64_t>
    {
        // Format: "bytes <start>-<end>/<total>" (total may be "*")
        if (contentRange.empty())
        {
            return std::nullopt;
        }

        const auto slash = contentRange.find('/');
        if (slash == std::string::npos || slash + 1 >= contentRange.size())
        {
            return std::nullopt;
        }

        const std::string totalPart = contentRange.substr(slash + 1);
        if (totalPart == "*")
        {
            return std::nullopt;
        }

        try
        {
            return static_cast<uint64_t>(std::stoull(totalPart));
        }
        catch (...)
        {
            return std::nullopt;
        }
    };

    auto getLocalSize = [&]() -> uint64_t
    {
        std::error_code ec;
        if (!release.localTempFilePath.empty() && std::filesystem::exists(release.localTempFilePath, ec) && !ec)
        {
            const auto sz = std::filesystem::file_size(release.localTempFilePath, ec);
            return ec ? 0ULL : static_cast<uint64_t>(sz);
        }
        return 0ULL;
    };

    while (true)
    {
        if (abortDownloadRequested.load(std::memory_order_relaxed))
        {
            spdlog::info("Download aborted before starting next attempt");
            return std::unexpected("Download cancelled.");
        }

        const auto ua = std::format("{}/{}", appFilename, appVersion.str());

        std::optional<uint64_t> expectedSize{};
        if (release.downloadSize.has_value())
        {
            expectedSize = static_cast<uint64_t>(release.downloadSize.value());
        }

        uint64_t localSize = getLocalSize();

        if (expectedSize.has_value() && localSize == expectedSize.value())
        {
            spdlog::info("Local file already complete ({} bytes), skipping download", localSize);
            return httplib::OK_200;
        }

        // If local size exceeds expected, drop it and start over
        if (expectedSize.has_value() && localSize > expectedSize.value())
        {
            spdlog::warn("Local temp file is larger than expected ({} > {}), truncating and restarting",
                         localSize, expectedSize.value());
            localSize = 0;
        }

        const bool wantsResume = localSize > 0;

        const std::ios::openmode openMode =
            std::ios::binary | (wantsResume ? std::ios::app : std::ios::trunc);

        try
        {
            outStream.open(release.localTempFilePath.string(), openMode);
        }
        catch (std::ios_base::failure& e)
        {
            spdlog::error("Failed to open file {}, error {}", release.localTempFilePath, e.what());
            return std::unexpected(std::format("Failed to open temporary file '{}': {}", release.localTempFilePath.string(), e.what()));
        }

        spdlog::debug("Starting release download from {} (timeout {}s)", release.downloadUrl, currentTimeoutSecs);
        if (wantsResume)
        {
            const auto rangeHeader = std::format("bytes={}-", localSize);
            spdlog::info("Attempting to resume download at offset {} (Range: {})", localSize, rangeHeader);
        }

        struct CurlHeaderCollector
        {
            std::unordered_map<std::string, std::string> fields{};
            long lastHttpCode{0};
            bool abortBecauseRangeIgnored{false};
        };

        CurlHeaderCollector headerCollector{};

        auto trimInPlace = [](std::string& s)
        {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
            while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) s.pop_back();
        };

        // Note: this curlpp version expects std::function callbacks WITHOUT userdata.
        // We capture required state in lambdas instead.
        auto headerCallback = [&](char* buffer, size_t size, size_t nitems) -> size_t
        {
            const size_t bytes = size * nitems;
            if (buffer == nullptr || bytes == 0)
            {
                return bytes;
            }

            std::string line(buffer, buffer + bytes);

            // Status line, e.g. "HTTP/1.1 206 Partial Content"
            if (line.rfind("HTTP/", 0) == 0)
            {
                const auto firstSpace = line.find(' ');
                if (firstSpace != std::string::npos && (firstSpace + 4) <= line.size())
                {
                    try
                    {
                        headerCollector.lastHttpCode = std::stol(line.substr(firstSpace + 1, 3));
                    }
                    catch (...)
                    {
                        // ignore
                    }
                }

                return bytes;
            }

            const auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                return bytes;
            }

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim whitespace and CRLF
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            while (!key.empty() && isSpace(static_cast<unsigned char>(key.back()))) key.pop_back();
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) value.pop_back();

            if (!key.empty())
            {
                headerCollector.fields[ key ] = value;
            }

            return bytes;
        };

        auto writeCallback = [&](char* ptr, size_t size, size_t nmemb) -> size_t
        {
            const size_t bytes = size * nmemb;
            if (ptr == nullptr || bytes == 0)
            {
                return bytes;
            }

            // If we attempted to resume but the server ignores Range (responds 200),
            // abort before writing any body bytes to avoid corrupting the local file.
            if (wantsResume && headerCollector.lastHttpCode == 200)
            {
                headerCollector.abortBecauseRangeIgnored = true;
                return 0;
            }

            try
            {
                outStream.write(ptr, static_cast<std::streamsize>(bytes));
                return bytes;
            }
            catch (...)
            {
                return 0; // abort transfer
            }
        };

        // Build common vendor + additional headers via the shared helper
        std::list<std::string> headerLines = BuildCommonHeaders();

        curlpp::Easy req;
        req.setOpt(curlpp::options::Url(release.downloadUrl));
        req.setOpt(curlpp::options::UserAgent(ua));
        req.setOpt(curlpp::options::FollowLocation(true));
        req.setOpt(curlpp::options::MaxRedirs(MAX_REDIRECTS));
        req.setOpt(curlpp::options::HttpHeader(headerLines));

        // Prefer "stall" timeouts: don't abort if bytes still trickle in.
        req.setOpt(curlpp::options::ConnectTimeout(60));
        req.setOpt(curlpp::options::LowSpeedLimit(1));
        req.setOpt(curlpp::options::LowSpeedTime(currentTimeoutSecs));

        // Streaming write + header collection
        req.setOpt(curlpp::options::WriteFunction(writeCallback));
        req.setOpt(curlpp::options::HeaderFunction(headerCallback));

        // Progress (always enabled so we can abort on shutdown)
        auto progressCallback = [this, progressFn](double dltotal, double dlnow, double ultotal, double ulnow) -> int
        {
            if (abortDownloadRequested.load(std::memory_order_relaxed))
            {
                return 1; // abort transfer
            }

            if (progressFn != nullptr)
            {
                return progressFn(nullptr, dltotal, dlnow, ultotal, ulnow);
            }

            return 0;
        };
        req.setOpt(curlpp::options::NoProgress(false));
        req.setOpt(curlpp::options::ProgressFunction(progressCallback));

        // Resume
        if (wantsResume)
        {
#ifdef CURLOPT_RESUME_FROM_LARGE
            req.setOpt(curlpp::options::ResumeFromLarge(static_cast<curl_off_t>(localSize)));
#else
            req.setOpt(curlpp::options::ResumeFrom(static_cast<long>(localSize)));
#endif
        }

        int code = httplib::OK_200;
        try
        {
            req.perform();

            // If we didn't parse an HTTP status line, treat as OK (best-effort).
            code = headerCollector.lastHttpCode != 0 ? static_cast<int>(headerCollector.lastHttpCode) : httplib::OK_200;
        }
        catch (const curlpp::RuntimeError& e)
        {
            if (abortDownloadRequested.load(std::memory_order_relaxed))
            {
                spdlog::info("Download aborted");
                return std::unexpected("Download cancelled.");
            }

            // If we aborted because the server ignored the Range request, restart from scratch.
            if (headerCollector.abortBecauseRangeIgnored)
            {
                spdlog::warn("Server ignored resume (returned 200), restarting from scratch");
                try
                {
                    outStream.close();
                    outStream.open(release.localTempFilePath.string(), std::ios::binary | std::ios::trunc);
                    outStream.close();
                }
                catch (...)
                {
                    // ignore
                }

                // Retry without consuming a retry slot.
                continue;
            }

            std::string msg = e.what();
            if (headerCollector.lastHttpCode != 0)
            {
                msg = std::format("HTTP {}: {}", headerCollector.lastHttpCode, msg);
            }

            lastFailureDetails = msg;

            // Best-effort mapping: detect timeout text to enable backoff.
            const bool isTimeout =
                msg.find("Timeout") != std::string::npos ||
                msg.find("timeout") != std::string::npos ||
                msg.find("timed out") != std::string::npos ||
                msg.find("Timed out") != std::string::npos;

            if (isTimeout)
            {
                code = static_cast<int>(CURLE_OPERATION_TIMEDOUT);
            }
            else if (headerCollector.lastHttpCode >= 400 && headerCollector.lastHttpCode < 600)
            {
                // Prefer the HTTP status if we have one, so UI can show status text.
                code = static_cast<int>(headerCollector.lastHttpCode);
            }
            else
            {
                // Generic cURL-ish failure so UI doesn't show an empty enum name.
                code = static_cast<int>(CURLE_RECV_ERROR);
            }
        }
        catch (const curlpp::LogicError& e)
        {
            spdlog::error("cURLpp logic error: {}", e.what());
            lastFailureDetails = e.what();
            code = static_cast<int>(CURLE_FAILED_INIT);
        }

        try { outStream.close(); } catch (...) { }

        const auto& headers = headerCollector.fields;

        // Update expected size from headers if not provided by server JSON
        if (!expectedSize.has_value())
        {
            if (const auto cr = tryGetHeader(headers, "Content-Range"); !cr.empty())
            {
                expectedSize = tryParseTotalSizeFromContentRange(cr);
            }
            else if (code == httplib::OK_200)
            {
                const auto cl = tryGetHeader(headers, "Content-Length");
                if (!cl.empty())
                {
                    try
                    {
                        expectedSize = static_cast<uint64_t>(std::stoull(cl));
                    }
                    catch (...)
                    {
                        // ignore
                    }
                }
            }
        }

        const uint64_t finalSize = getLocalSize();

        auto tryExtractFilenameFromContentDisposition = [](const std::string& cdHeader) -> std::optional<std::filesystem::path>
        {
            if (cdHeader.empty())
            {
                return std::nullopt;
            }

            std::vector<std::string> arguments;
            char dl = ';';
            size_t start = 0, end = 0;

            while ((start = cdHeader.find_first_not_of(dl, end)) != std::string::npos)
            {
                end = cdHeader.find(dl, start);
                arguments.push_back(cdHeader.substr(start, end - start));
            }

            if (arguments.size() < 2)
            {
                return std::nullopt;
            }

            const auto& filename = arguments[ 1 ];
            std::regex productRegex("filename=(.*)", std::regex_constants::icase);
            auto matchesBegin = std::sregex_iterator(filename.begin(), filename.end(), productRegex);
            auto matchesEnd = std::sregex_iterator();

            if (matchesBegin == matchesEnd)
            {
                return std::nullopt;
            }

            const std::smatch& match = *matchesBegin;
            return std::filesystem::path(match[ 1 ].str());
        };

        // Treat 206 as success only if file size matches expected
        if (code == httplib::PartialContent_206)
        {
            if (expectedSize.has_value() && finalSize == expectedSize.value())
            {
                spdlog::info("Resumed download completed successfully ({} bytes)", finalSize);
                code = httplib::OK_200; // normalize
            }
            else
            {
                spdlog::warn("Partial Content received but file is incomplete ({} of {}), will retry",
                             finalSize, expectedSize.value_or(0ULL));
                lastFailureDetails = std::format("HTTP 206 Partial Content but file is incomplete ({} of {})",
                                                 finalSize, expectedSize.value_or(0ULL));
            }
        }

        // If we attempted to resume but server says the range is not satisfiable, restart download.
        if (wantsResume && code == httplib::RangeNotSatisfiable_416)
        {
            spdlog::warn("Server responded 416 to Range request, truncating and restarting");
            try
            {
                outStream.open(release.localTempFilePath.string(), std::ios::binary | std::ios::trunc);
                outStream.close();
            }
            catch (std::ios_base::failure& e)
            {
                spdlog::error("Failed to truncate file {}, error {}", release.localTempFilePath, e.what());
                return std::unexpected(std::format("Failed to truncate temporary file for restart: {}", e.what()));
            }
        }

        // try to grab original filename (may be missing on some retries/ranged requests)
        const auto cd = tryGetHeader(headers, "Content-Disposition");
        if (!cd.empty())
        {
            spdlog::debug("Content-Disposition header value: {}", cd);
            if (auto parsed = tryExtractFilenameFromContentDisposition(cd); parsed.has_value())
            {
                cachedAttachmentName = parsed.value();
            }
        }
        else
        {
            spdlog::debug("Content-Disposition header value: <empty>");
        }

        // attempt to get true filename (use cached name if this attempt omitted it)
        if (code == httplib::OK_200)
        {
            std::optional<std::filesystem::path> attachmentName{};
            if (auto parsed = tryExtractFilenameFromContentDisposition(cd); parsed.has_value())
            {
                attachmentName = parsed.value();
            }
            else if (cachedAttachmentName.has_value())
            {
                attachmentName = cachedAttachmentName.value();
            }

            if (attachmentName.has_value())
            {
                std::filesystem::path newLocation = release.localTempFilePath;
                newLocation.replace_filename(attachmentName.value());

                // If the server-supplied filename already matches our current path, don't "rename".
                // Otherwise we'd delete the file and then fail to move it (ERROR_FILE_NOT_FOUND).
                if (newLocation == release.localTempFilePath)
                {
                    spdlog::debug("Skipping rename; already named {}", newLocation);
                }
                else
                {
                    spdlog::debug("Renaming {} to {}", release.localTempFilePath, newLocation);

                    DeleteFileA(newLocation.string().c_str());

                    // some setups with bootstrappers (like InnoSetup) require the original .exe extension
                    // otherwise it will fail to launch itself elevated with a "ShellExecuteEx failed" error.
                    if (!MoveFileA(release.localTempFilePath.string().c_str(), newLocation.string().c_str()))
                    {
                        spdlog::error("Failed to rename {} to {}, error: {:#x}", release.localTempFilePath,
                                      newLocation, GetLastError());
                    }
                    else
                    {
                        release.localTempFilePath = newLocation;
                    }
                }
            }
        }

        if (code == httplib::OK_200)
        {
            return code;
        }

        if (code == httplib::NotFound_404)
        {
            spdlog::error("GET request failed with 404, deleting temporary file {}", release.localTempFilePath);
            if (DeleteFileA(release.localTempFilePath.string().c_str()) == FALSE)
            {
                spdlog::warn("Failed to delete temporary file {}, error {:#x}, message {}", release.localTempFilePath,
                             GetLastError(), winapi::GetLastErrorStdStr());
            }
            return std::unexpected("HTTP 404: Not Found");
        }

        if (--retryCount > 0)
        {
            spdlog::debug("Web request failed (code {}), retrying {} more time(s)", code, retryCount);

            if (code == CURLE_OPERATION_TIMEDOUT)
            {
                currentTimeoutSecs = std::min(currentTimeoutSecs * 2, kMaxTimeoutSecs);
                spdlog::info("Request timeout reached, setting new timeout to {} seconds", currentTimeoutSecs);
            }

            std::mt19937_64 eng{std::random_device{}()};
            std::uniform_int_distribution<> dist{1000, 5000};
            std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
            continue;
        }

        auto errorMessage = magic_enum::enum_name<CURLcode>(static_cast<CURLcode>(code));
        const std::string finalErrorMsg = !lastFailureDetails.empty()
                                              ? lastFailureDetails
                                              : (errorMessage.empty() ? httplib::status_message(code) : std::string(errorMessage));
        spdlog::error("GET request failed with code {}, message {}", code, finalErrorMsg);

        // Keep partial file to allow resuming on next user retry (only remove on 404 above)
        return std::unexpected(finalErrorMsg);
    }
}

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    /**
     * \brief Returns true if the URL uses HTTPS (or, in debug builds, HTTP localhost).
     * Rejects http://, file://, and any other non-HTTPS scheme.
     */
    bool IsAllowedDownloadUrl(const std::string& url)
    {
        if (url.empty()) return false;

        // HTTPS is always allowed
        if (url.rfind("https://", 0) == 0) return true;

#if !defined(NDEBUG) || defined(NV_FLAGS_ALLOW_HTTP_DOWNLOAD)
        // In debug builds, allow plain HTTP for local test servers
        if (url.rfind("http://localhost", 0) == 0 ||
            url.rfind("http://127.0.0.1", 0) == 0 ||
            url.rfind("http://[::1]", 0) == 0)
        {
            return true;
        }
#endif

        return false;
    }
}

// ============================================================================
// RequestUpdateInfo
// ============================================================================

[[nodiscard]] std::expected<void, std::string> models::InstanceConfig::RequestUpdateInfo()
{
    const auto ua = std::format("{}/{}", appFilename, appVersion.str());
    spdlog::debug("Setting User Agent to {}", ua);

    long currentTimeoutSecs = MAX_TIMEOUT_SECS;

    // Assemble per-request headers once; Accept header prepended here.
    auto buildManifestHeaders = [&]() -> std::list<std::string>
    {
        auto lines = BuildCommonHeaders();
        lines.push_front("Accept: application/json");
        return lines;
    };

    std::vector<std::string> candidateUrls;
    candidateUrls.reserve(1 + fallbackUpdateRequestUrls.size());
    candidateUrls.push_back(updateRequestUrl);
    candidateUrls.insert(candidateUrls.end(), fallbackUpdateRequestUrls.begin(), fallbackUpdateRequestUrls.end());

    // Best-effort: try primary URL first, then fallbacks if the primary is not accessible
    for (size_t i = 0; i < candidateUrls.size(); i++)
    {
        const auto& requestUrl = candidateUrls[ i ];

        if (!IsAllowedDownloadUrl(requestUrl))
        {
            spdlog::error("Manifest URL {} uses a disallowed scheme and will not be fetched", requestUrl);
            // Treat as unavailable and try next candidate; if none remain, the
            // outer loop exits and the caller receives the "no server reachable" error.
            continue;
        }

        spdlog::info("Requesting update info from {} ({}/{})", requestUrl, i + 1, candidateUrls.size());

        // ReSharper disable once CppTooWideScopeInitStatement
        int retryCount = 5;
        currentTimeoutSecs = MAX_TIMEOUT_SECS;

        while (true)
        {
            auto getResult = web::HttpGet(requestUrl, {
                .userAgent          = ua,
                .headers            = buildManifestHeaders(),
                .timeoutSecs        = currentTimeoutSecs,
                .connectTimeoutSecs = 60,
                .maxRedirects       = MAX_REDIRECTS,
            });

            // Transport failure (connection error, TLS failure, stall timeout, …)
            if (!getResult)
            {
                const std::string& transportError = getResult.error();
                const bool isTimeout =
                    transportError.find("timeout") != std::string::npos ||
                    transportError.find("Timeout") != std::string::npos ||
                    transportError.find("timed out") != std::string::npos ||
                    transportError.find("Timed out") != std::string::npos;

                if (--retryCount > 0)
                {
                    spdlog::debug("Web request failed ({}), retrying {} more time(s)", transportError, retryCount);

                    std::mt19937_64 eng{std::random_device{}()};
                    std::uniform_int_distribution<> dist{1000, 5000};
                    std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});

                    if (isTimeout)
                    {
                        long nxt = std::lround(currentTimeoutSecs * 1.5);
                        currentTimeoutSecs = std::min(nxt, 900L);
                        spdlog::info("Request timeout reached, setting new timeout to {} seconds", currentTimeoutSecs);
                    }

                    continue;
                }

                spdlog::error("GET request failed: {}", transportError);

                if ((i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Primary URL not accessible ({}), attempting fallback", transportError);
                    break; // try next candidate URL
                }

                return std::unexpected(transportError);
            }

            const auto& res  = *getResult;
            const int    code = static_cast<int>(res.httpCode);
            const std::string& body = res.body;

            // HTTP-level failure (server reachable but returned an error status)
            if (code != httplib::OK_200)
            {
                if (code != httplib::NotFound_404 && --retryCount > 0)
                {
                    spdlog::debug("Web request failed (HTTP {}), retrying {} more time(s)", code, retryCount);

                    std::mt19937_64 eng{std::random_device{}()};
                    std::uniform_int_distribution<> dist{1000, 5000};
                    std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
                    continue;
                }

                const std::string statusText = httplib::status_message(code);
                spdlog::error("GET request failed with HTTP {}: {}", code, statusText);

                if ((i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Manifest URL returned HTTP {}, attempting fallback", code);
                    break; // try next candidate URL
                }

                return std::unexpected(std::format("HTTP {} {}", code, statusText));
            }

            try
            {
                //
                // Layer 3: Manifest signature verification (Ed25519 / minisign)
                // Must happen BEFORE json::parse so a tampered body is never trusted.
                //
#if defined(NV_MANIFEST_PUBLIC_KEY)
                {
                    // Derive the .minisig sidecar URL by appending ".minisig" to the manifest URL
                    const std::string minisigUrl = requestUrl + ".minisig";
                    spdlog::debug("Fetching manifest signature from {}", minisigUrl);

                    auto sigGetResult = web::HttpGet(minisigUrl, {
                        .userAgent          = ua,
                        .headers            = BuildCommonHeaders(),
                        .timeoutSecs        = MAX_TIMEOUT_SECS,
                        .connectTimeoutSecs = 60,
                        .maxRedirects       = MAX_REDIRECTS,
                    });

                    const bool sigOk = sigGetResult.has_value()
                        && sigGetResult->httpCode == httplib::OK_200
                        && !sigGetResult->body.empty();

                    if (!sigOk)
                    {
                        const std::string reason = sigGetResult
                            ? std::format("HTTP {}", sigGetResult->httpCode)
                            : sigGetResult.error();
                        spdlog::error("Failed to fetch manifest signature from {} ({})", minisigUrl, reason);

                        if ((i + 1) < candidateUrls.size())
                        {
                            spdlog::warn("Manifest signature unavailable from {}, attempting fallback", requestUrl);
                            break; // try next candidate URL
                        }

                        return std::unexpected(
                            "Manifest signature (.minisig) could not be fetched. "
                            "Update blocked because NV_MANIFEST_PUBLIC_KEY is configured.");
                    }

                    const auto sigResult = VerifyManifestSignature(body, sigGetResult->body);
                    if (!sigResult)
                    {
                        spdlog::error("Manifest signature verification failed: {}", sigResult.error());
                        return std::unexpected(std::format("Manifest signature invalid: {}", sigResult.error()));
                    }

                    spdlog::info("Manifest signature verified successfully");
                }
#endif

                const json reply = json::parse(body);
                remote = reply.get<UpdateResponse>();

                // remove releases marked as disabled
                std::erase_if(remote.releases, [](const UpdateRelease& x) { return x.disabled.value_or(false); });

                // top release is always latest by version, even if the response wasn't the right order
                std::ranges::sort(remote.releases, [](const UpdateRelease& lhs, const UpdateRelease& rhs)
                {
                    return lhs.GetSemVersion() > rhs.GetSemVersion();
                });

                //
                // Layer 3: Rollback / downgrade protection
                //
#if defined(NV_MANIFEST_PUBLIC_KEY)
                if (reply.contains("manifestVersion") && reply["manifestVersion"].is_number_unsigned())
                {
                    const uint64_t manifestVersion = reply["manifestVersion"].get<uint64_t>();
                    if (!CheckAndUpdateManifestVersion(manifestVersion))
                    {
                        spdlog::error("Manifest version rollback detected (version {}), blocking update", manifestVersion);
                        return std::unexpected(
                            "The update manifest appears to be a downgrade attempt and has been rejected.");
                    }
                }
#endif

                //
                // Layer 5: Enforce HTTPS on all download URLs from the manifest
                //
                for (const auto& release : remote.releases)
                {
                    if (!IsAllowedDownloadUrl(release.downloadUrl))
                    {
                        spdlog::error("Release '{}' has a disallowed downloadUrl scheme: {}",
                                      release.name, release.downloadUrl);
                        return std::unexpected(
                            std::format("Release '{}' uses a non-HTTPS downloadUrl which is not allowed for security reasons.",
                                        release.name));
                    }
                }

                if (remote.instance.has_value())
                {
                    const auto& inst = remote.instance.value();
                    if (inst.latestUrl.has_value() && !IsAllowedDownloadUrl(inst.latestUrl.value()))
                    {
                        spdlog::error("instance.latestUrl has a disallowed scheme: {}", inst.latestUrl.value());
                        return std::unexpected(
                            "The self-updater URL (instance.latestUrl) uses a non-HTTPS scheme which is not allowed.");
                    }
                }

                // bail out now if we are not supposed to obey the server settings
                if (authority == Authority::Local || !reply.contains("shared"))
                {
                    spdlog::info("{} authority specified (or empty response), ignoring server parameters",
                                 magic_enum::enum_name(authority));
                    return {};
                }

                // merge values that can be supplied both locally and remotely
                if (remote.shared.has_value())
                {
                    const auto& shared = remote.shared.value();
                    spdlog::info("Processing remote shared configuration parameters");

                    if (shared.windowTitle.has_value()) merged.windowTitle = shared.windowTitle.value();

                    if (shared.productName.has_value()) merged.productName = shared.productName.value();

                    // special case where we don't want the server to override what the CLI specified
                    if (!this->forceLocalVersion)
                    {
                        if (shared.detectionMethod.has_value()) merged.detectionMethod = shared.detectionMethod.value();

                        if (shared.detection.has_value()) merged.detection = shared.detection.value();
                    }

                    if (shared.installationErrorUrl.has_value())
                        merged.installationErrorUrl = shared.installationErrorUrl.value();

                    if (shared.downloadLocation.has_value()) merged.downloadLocation = shared.downloadLocation.value();

                    if (shared.runAsTemporaryCopy.has_value()) merged.runAsTemporaryCopy = shared.runAsTemporaryCopy.value();

                    // Signature verification settings from server (do not override strict mode set by CLI)
                    if (shared.signatureVerificationMode.has_value() && !strictVerification)
                        merged.signatureVerificationMode = shared.signatureVerificationMode.value();

                    if (shared.signaturePolicy.has_value() && !strictVerification)
                        merged.signaturePolicy = shared.signaturePolicy.value();

                    if (shared.signatureStrategy.has_value() && !strictVerification)
                        merged.signatureStrategy = shared.signatureStrategy.value();

                    if (shared.signatureConfig.has_value() && !strictVerification)
                        merged.signatureConfig = shared.signatureConfig.value();

                    if (shared.hideRemindButton.has_value()) merged.hideRemindButton = shared.hideRemindButton.value();
                }

                return {};
            }
            catch (const json::exception& e)
            {
                spdlog::error("Failed to parse JSON, error {}", e.what());

                // Some censorship setups return HTTP 200 with non-JSON body (block pages).
                // If configured, try fallbacks before failing hard.
                if ((i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Non-JSON response received, attempting fallback");
                    break; // try next candidate URL
                }

                return std::unexpected(std::format("JSON parsing error: {}", e.what()));
            }
            catch (const std::exception& e)
            {
                spdlog::error("Unexpected error during response parsing, error {}", e.what());

                if ((i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Response parsing failed, attempting fallback");
                    break; // try next candidate URL
                }

                return std::unexpected(std::format("Unknown error error: {}", e.what()));
            }
        }
    }

    return std::unexpected("No update server URL could be reached");
}
