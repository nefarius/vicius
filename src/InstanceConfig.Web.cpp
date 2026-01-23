#include "pch.h"
#include "Util.h"
#include "InstanceConfig.hpp"


void models::InstanceConfig::SetCommonHeaders(_Inout_ RestClient::Connection* conn) const
{
    //
    // If a backend server is used, it can alter the response based on
    // these header values, classic web servers will just ignore them
    //

#if !defined(NV_FLAGS_NO_VENDOR_HEADERS)
    conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-Manufacturer", manufacturer);
    conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-Product", product);
    conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-Version", appVersion.str());
    if (!channel.empty())
    {
        conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-Channel", channel);
    }

    //
    // Report updater process architecture
    //

    conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-Process-Architecture",
#if defined(_M_AMD64)
                       "x64"
#elif defined(_M_ARM64)
                       "arm64"
#else
                       "x86"
#endif
        );

    //
    // Report OS/CPU architecture
    //

    const SYSTEM_INFO si = nefarius::winapi::SafeGetNativeSystemInfo();

    std::string arch;

    switch (si.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "x64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = "arm64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "x86";
            break;
        default:
            arch = "<unknown>";
            break;
    }

    conn->AppendHeader("X-" NV_HTTP_HEADERS_NAME "-OS-Architecture", arch);

    //
    // Custom headers passed via CLI
    //

    for (const auto& kvp : additionalHeaders)
    {
        const auto name = std::format("X-" NV_HTTP_HEADERS_NAME "-{}", kvp.first);
        const auto value = kvp.second;
        conn->AppendHeader(name, value);
    }

#endif
}

int models::InstanceConfig::DownloadRelease(curl_progress_callback progressFn, const int releaseIndex)
{
    UNREFERENCED_PARAMETER(releaseIndex);

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

            winapi::GetUserTemporaryDirectory(downloadLocation);
        }
    }
    else
    {
        winapi::GetUserTemporaryDirectory(downloadLocation);
    }

    // if we're still empty, the previous methods all failed
    if (downloadLocation.empty())
    {
        std::string programDataDir;

        // fallback to writable location for all users
        if (!winapi::GetProgramDataPath(programDataDir))
        {
            spdlog::error("Failed to get %ProgramData% directory, error: {0:#x}", GetLastError());
            return -1;
        }

        // build new absolute path, e.g. "C:\ProgramData\nefarius\HidHide\downloads" or "C:\ProgramData\Updater\downloads"
        const std::filesystem::path subDir = manufacturer.empty() ? appFilename : std::filesystem::path(manufacturer) / product;
        const std::filesystem::path targetDirectory = std::filesystem::path(programDataDir) / subDir / "downloads";

        // ensure this location can be created
        if (!nefarius::winapi::fs::DirectoryCreate(targetDirectory.string()))
        {
            spdlog::error("Failed to create fallback download location {}, error: {:#x}", targetDirectory,
                          GetLastError());
            return -1;
        }

        downloadLocation = targetDirectory.string();
    }

    std::string tempFile{};

    auto& release = GetSelectedRelease();

    // Reuse existing temp file if present (allows resuming across user retries)
    if (release.localTempFilePath.empty())
    {
        if (!winapi::GetNewTemporaryFile(tempFile, downloadLocation))
        {
            spdlog::error("Failed to get temporary file name");
            return -1;
        }

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

    // ReSharper disable once CppTooWideScopeInitStatement
    int retryCount = 5;
    long currentTimeoutSecs = MAX_TIMEOUT_SECS;
    constexpr long kMaxTimeoutSecs = 3600; // 1 hour

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
        // Recreate connection for each attempt to avoid stale curl state
        auto conn = std::make_unique<RestClient::Connection>("");

        const auto ua = std::format("{}/{}", appFilename, appVersion.str());
        conn->SetUserAgent(ua);
        conn->FollowRedirects(true, MAX_REDIRECTS);
        conn->SetTimeout(currentTimeoutSecs);
        conn->SetFileProgressCallback(progressFn);

        SetCommonHeaders(conn.get());

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

        if (wantsResume)
        {
            const auto rangeHeader = std::format("bytes={}-", localSize);
            spdlog::info("Attempting to resume download at offset {} (Range: {})", localSize, rangeHeader);
            conn->AppendHeader("Range", rangeHeader);
        }

        const std::ios::openmode openMode =
            std::ios::binary | (wantsResume ? std::ios::app : std::ios::trunc);

        try
        {
            outStream.open(release.localTempFilePath.string(), openMode);
        }
        catch (std::ios_base::failure& e)
        {
            spdlog::error("Failed to open file {}, error {}", release.localTempFilePath, e.what());
            return -1;
        }

        spdlog::debug("Starting release download from {} (timeout {}s)", release.downloadUrl, currentTimeoutSecs);

        auto [ code, body, headers ] = conn->get(release.downloadUrl);

        // Special case: if we attempted a resume but the server ignored the Range header and returned 200,
        // restart locally by truncating and writing the full body.
        if (wantsResume && code == httplib::OK_200)
        {
            spdlog::warn("Server ignored Range request (returned 200), restarting download from scratch");
            outStream.close();
            try
            {
                outStream.open(release.localTempFilePath.string(), std::ios::binary | std::ios::trunc);
            }
            catch (std::ios_base::failure& e)
            {
                spdlog::error("Failed to reopen file {}, error {}", release.localTempFilePath, e.what());
                return -1;
            }
        }

        if (!body.empty())
        {
            outStream.write(body.data(), body.size());
        }
        outStream.close();

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
                return -1;
            }
        }

        // try to grab original filename
        const auto cd = tryGetHeader(headers, "Content-Disposition");

        spdlog::debug("Content-Disposition header value: {}", cd);

        // attempt to get true filename
        if (code == httplib::OK_200 && !cd.empty())
        {
            std::vector<std::string> arguments;
            char dl = ';';
            size_t start = 0, end = 0;

            // extract tokens
            while ((start = cd.find_first_not_of(dl, end)) != std::string::npos)
            {
                end = cd.find(dl, start);
                arguments.push_back(cd.substr(start, end - start));
            }

            if (arguments.size() >= 2)
            {
                const auto& filename = arguments[ 1 ];

                std::regex productRegex("filename=(.*)", std::regex_constants::icase);
                auto matchesBegin = std::sregex_iterator(filename.begin(), filename.end(), productRegex);
                auto matchesEnd = std::sregex_iterator();

                if (matchesBegin != matchesEnd)
                {
                    const std::smatch& match = *matchesBegin;
                    std::filesystem::path attachmentName(match[ 1 ].str());

                    std::filesystem::path newLocation = release.localTempFilePath;
                    //newLocation.replace_extension(attachmentName.extension());
                    newLocation.replace_filename(attachmentName);

                    // If the server-supplied filename already matches our current path, don't "rename".
                    // Otherwise we'd delete the file and then fail to move it (ERROR_FILE_NOT_FOUND).
                    if (newLocation == release.localTempFilePath)
                    {
                        spdlog::debug("Skipping rename; already named {}", newLocation);
                        continue;
                    }

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
            return code;
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
        errorMessage = errorMessage.empty() ? httplib::status_message(code) : errorMessage;
        spdlog::error("GET request failed with code {}, message {}", code, errorMessage);

        // Keep partial file to allow resuming on next user retry (only remove on 404 above)
        return code;
    }
}

[[nodiscard]] std::tuple<bool, std::string> models::InstanceConfig::RequestUpdateInfo()
{
    const auto conn = new RestClient::Connection("");

    const auto ua = std::format("{}/{}", appFilename, appVersion.str());
    spdlog::debug("Setting User Agent to {}", ua);
    conn->SetUserAgent(ua);
    conn->FollowRedirects(true, MAX_REDIRECTS);
    int currentTimeoutSecs = MAX_TIMEOUT_SECS;

    RestClient::HeaderFields headers;
    headers[ "Accept" ] = "application/json";
    conn->SetHeaders(headers);

    SetCommonHeaders(conn);

    std::vector<std::string> candidateUrls;
    candidateUrls.reserve(1 + fallbackUpdateRequestUrls.size());
    candidateUrls.push_back(updateRequestUrl);
    candidateUrls.insert(candidateUrls.end(), fallbackUpdateRequestUrls.begin(), fallbackUpdateRequestUrls.end());

    // Best-effort: try primary URL first, then fallbacks if the primary is not accessible
    for (size_t i = 0; i < candidateUrls.size(); i++)
    {
        const auto& requestUrl = candidateUrls[ i ];
        spdlog::info("Requesting update info from {} ({}/{})", requestUrl, i + 1, candidateUrls.size());

        // ReSharper disable once CppTooWideScopeInitStatement
        int retryCount = 5;
        currentTimeoutSecs = MAX_TIMEOUT_SECS;

        while (true)
        {
            conn->SetTimeout(currentTimeoutSecs);

            auto [ code, body, _ ] = conn->get(requestUrl);

            if (code != httplib::OK_200)
            {
                if (code != httplib::NotFound_404 && --retryCount > 0)
                {
                    spdlog::debug("Web request failed (code {}), retrying {} more time(s)", code, retryCount);

                    std::mt19937_64 eng{std::random_device{}()};
                    std::uniform_int_distribution<> dist{1000, 5000};
                    std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});

                    if (code == CURLE_OPERATION_TIMEDOUT)
                    {
                        long nxt = std::lround(currentTimeoutSecs * 1.5);
                        currentTimeoutSecs = std::min(nxt, 900L);
                        spdlog::info("Request timeout reached, setting new timeout to {} seconds", currentTimeoutSecs);
                    }

                    continue;
                }

                auto errorMessage = magic_enum::enum_name<CURLcode>(static_cast<CURLcode>(code));
                errorMessage = errorMessage.empty() ? httplib::status_message(code) : errorMessage;
                spdlog::error("GET request failed with code {}, message {}", code, errorMessage);

                // If the error is a cURL error (not an HTTP status), attempt fallbacks if available
                if (code > 0 && code < CURL_LAST && (i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Primary URL not accessible ({}), attempting fallback", errorMessage);
                    break; // try next candidate URL
                }

                return std::make_tuple(false, std::format("HTTP error {}", errorMessage));
            }

            try
            {
                const json reply = json::parse(body);
                remote = reply.get<UpdateResponse>();

                // remove releases marked as disabled
                std::erase_if(remote.releases, [](const UpdateRelease& x) { return x.disabled.value_or(false); });

                // top release is always latest by version, even if the response wasn't the right order
                std::ranges::sort(remote.releases, [](const UpdateRelease& lhs, const UpdateRelease& rhs)
                {
                    return lhs.GetSemVersion() > rhs.GetSemVersion();
                });

                // bail out now if we are not supposed to obey the server settings
                if (authority == Authority::Local || !reply.contains("shared"))
                {
                    spdlog::info("{} authority specified (or empty response), ignoring server parameters",
                                 magic_enum::enum_name(authority));
                    return std::make_tuple(true, "OK");
                }

                // merge values that can be supplied both locally end remotely
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
                }

                return std::make_tuple(true, "OK");
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

                return std::make_tuple(false, std::format("JSON parsing error: {}", e.what()));
            }
            catch (const std::exception& e)
            {
                spdlog::error("Unexpected error during response parsing, error {}", e.what());

                if ((i + 1) < candidateUrls.size())
                {
                    spdlog::warn("Response parsing failed, attempting fallback");
                    break; // try next candidate URL
                }

                return std::make_tuple(false, std::format("Unknown error error: {}", e.what()));
            }
        }
    }

    return std::make_tuple(false, "No update server URL could be reached");
}
