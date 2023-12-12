#include "pch.h"
#include "Util.h"
#include "InstanceConfig.hpp"
#include "MimeTypes.h"


void models::InstanceConfig::SetCommonHeaders(RestClient::Connection* conn) const
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

    conn->AppendHeader(
        "X-" NV_HTTP_HEADERS_NAME "-Process-Architecture",
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

    SYSTEM_INFO si = {};

    if (winapi::SafeGetNativeSystemInfo(&si))
    {
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
    }

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
    const auto conn = new RestClient::Connection("");

    conn->SetTimeout(5);
    const auto ua = std::format("{}/{}", appFilename, appVersion.str());
    spdlog::debug("Setting User Agent to {}", ua);
    conn->SetUserAgent(ua);
    conn->FollowRedirects(true, 5);
    conn->SetFileProgressCallback(progressFn);

    SetCommonHeaders(conn);

    std::string downloadLocation;

    if (!merged.downloadLocation.input.empty())
    {
        const auto& rendered = RenderInjaTemplate(merged.downloadLocation.input, merged.downloadLocation.data);

        if (winapi::DirectoryCreate(rendered))
        {
            downloadLocation = rendered;
        }
        else
        {
            spdlog::error("Failed to create download location {}, defaulting to TEMP path",
                          rendered);

            winapi::GetUserTemporaryPath(downloadLocation);
        }
    }
    else
    {
        winapi::GetUserTemporaryPath(downloadLocation);
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
        const std::filesystem::path subDir =
            manufacturer.empty()
                ? appFilename
                : std::filesystem::path(manufacturer) / product;
        const std::filesystem::path targetDirectory = std::filesystem::path(programDataDir) / subDir / "downloads";

        // ensure this location can be created
        if (!winapi::DirectoryCreate(targetDirectory.string()))
        {
            spdlog::error("Failed to create fallback download location {}, error: {:#x}",
                          targetDirectory.string(), GetLastError());
            return -1;
        }

        downloadLocation = targetDirectory.string();
    }

    std::string tempFile(MAX_PATH, '\0');

    if (GetTempFileNameA(downloadLocation.c_str(), "VICIUS", 0, tempFile.data()) == 0)
    {
        spdlog::error("Failed to get temporary file name, error: {:#x}", GetLastError());
        return -1;
    }

    util::stripNulls(tempFile);

    spdlog::debug("tempFile = {}", tempFile);

    auto& release = GetSelectedRelease();
    release.localTempFilePath = tempFile;

    // this is ugly but only one download can run in parallel so we're fine :)
    static std::ofstream outStream;

    const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
    outStream.exceptions(exceptionMask);

    try
    {
        outStream.open(release.localTempFilePath.string(), std::ios::binary);
    }
    catch (std::ios_base::failure& e)
    {
        spdlog::error("Failed to open file {}, error {}",
                      release.localTempFilePath.string(), e.what());
        return -1;
    }

    // write to file as we download it
    auto writeCallback = [](void* data, size_t size, size_t nmemb, void* userdata) -> size_t
    {
        UNREFERENCED_PARAMETER(userdata);

        const auto bytes = size * nmemb;

        // TODO: error handling
        outStream.write(static_cast<char*>(data), bytes);

        return bytes;
    };

    conn->SetWriteFunction(writeCallback);

    auto [code, body, headers] = conn->get(release.downloadUrl);

    outStream.close();

    // try to grab original filename
    const auto& cd = headers["Content-Disposition"];

    // attempt to get true filename
    if (!cd.empty())
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
            const auto& filename = arguments[1];

            std::regex productRegex("filename=(.*)", std::regex_constants::icase);
            auto matchesBegin = std::sregex_iterator(filename.begin(), filename.end(), productRegex);
            auto matchesEnd = std::sregex_iterator();

            if (matchesBegin != matchesEnd)
            {
                const std::smatch& match = *matchesBegin;
                std::filesystem::path attachmentName(match[1].str());

                std::filesystem::path newLocation = release.localTempFilePath;
                //newLocation.replace_extension(attachmentName.extension());
                newLocation.replace_filename(attachmentName);

                spdlog::debug("Renaming {} to {}", release.localTempFilePath.string(), newLocation.string());

                // some setups with bootstrappers (like InnoSetup) require the original .exe extension
                // otherwise it will fail to launch itself elevated with a "ShellExecuteEx failed" error.
                if (!MoveFileA(release.localTempFilePath.string().c_str(), newLocation.string().c_str()))
                {
                    spdlog::error("Failed to rename {} to {}, error: {:#x}",
                                  release.localTempFilePath.string(), newLocation.string(), GetLastError());
                }
                else
                {
                    release.localTempFilePath = newLocation;
                }
            }
        }
    }

    // TODO: error handling, retry?
    if (code != 200)
    {
        spdlog::error("GET request failed with code {}", code);

        // clean up local file since we re-download it when the user decides to retry
        if (DeleteFileA(release.localTempFilePath.string().c_str()) == 0)
        {
            spdlog::warn("Failed to delete temporary file {}, error {:#x}, message {}",
                         release.localTempFilePath.string(), GetLastError(), winapi::GetLastErrorStdStr());
        }
    }

    return code;
}

[[nodiscard]] std::tuple<bool, std::string> models::InstanceConfig::RequestUpdateInfo()
{
    const auto conn = new RestClient::Connection("");

    conn->SetTimeout(5);
    const auto ua = std::format("{}/{}", appFilename, appVersion.str());
    spdlog::debug("Setting User Agent to {}", ua);
    conn->SetUserAgent(ua);
    conn->FollowRedirects(true);
    conn->FollowRedirects(true, 5);

    RestClient::HeaderFields headers;
    headers["Accept"] = "application/json";
    conn->SetHeaders(headers);

    SetCommonHeaders(conn);

    auto [code, body, _] = conn->get(updateRequestUrl);

    if (code != 200)
    {
        // TODO: add retry logic or similar

        spdlog::error("GET request failed with code {}", code);
        const auto curlCode = magic_enum::enum_name<CURLcode>(static_cast<CURLcode>(code));
        return std::make_tuple(false, std::format("HTTP error {}", curlCode));
    }

    try
    {
        const json reply = json::parse(body);
        remote = reply.get<UpdateResponse>();

        // remove releases marked as disabled
        std::erase_if(
            remote.releases,
            [](const UpdateRelease& x)
            {
                return x.disabled;
            });

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

        if (remote.shared.has_value())
        {
            const auto& shared = remote.shared.value();
            spdlog::info("Processing remote shared configuration parameters");

            if (shared.windowTitle.has_value())
                merged.windowTitle = shared.windowTitle.value();

            if (shared.productName.has_value())
                merged.productName = shared.productName.value();

            if (shared.detectionMethod.has_value())
                merged.detectionMethod = shared.detectionMethod.value();

            if (shared.detection.has_value())
                merged.detection = shared.detection.value();

            if (shared.installationErrorUrl.has_value())
                merged.installationErrorUrl = shared.installationErrorUrl.value();

            if (shared.downloadLocation.has_value())
                merged.downloadLocation = shared.downloadLocation.value();
        }

        return std::make_tuple(true, "OK");
    }
    catch (const json::exception& e)
    {
        spdlog::error("Failed to parse JSON, error {}", e.what());
        return std::make_tuple(false, std::format("JSON parsing error: {}", e.what()));
    }
    catch (const std::exception& e)
    {
        spdlog::error("Unexpected error during response parsing, error {}", e.what());
        return std::make_tuple(false, std::format("Unknown error error: {}", e.what()));
    }
}
