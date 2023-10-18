#include "pch.h"
#include "Common.h"
#include "InstanceConfig.hpp"


models::InstanceConfig::InstanceConfig(HINSTANCE hInstance, argh::parser& cmdl) : appInstance(hInstance), remote()
{
    RestClient::init();

    //
    // Setup logger
    // 

    const auto logLevel = magic_enum::enum_cast<spdlog::level::level_enum>(cmdl("--log-level").str());

    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);

    // override log level, if provided by CLI
    if (logLevel.has_value())
    {
        sink->set_level(logLevel.value());
    }
    else
    {
        sink->set_level(spdlog::level::info);
    }

    auto logger = std::make_shared<spdlog::logger>("vicius-updater", sink);

    // override log level, if provided by CLI
    if (logLevel.has_value())
    {
        logger->set_level(logLevel.value());
        logger->flush_on(logLevel.value());
    }
    else
    {
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
    }

    set_default_logger(logger);

    spdlog::debug("Initializing updater instance (PID: {})", GetCurrentProcessId());

    //
    // Defaults and embedded stuff
    // 

    isSilent = cmdl[{NV_CLI_BACKGROUND}] || cmdl[{NV_CLI_SILENT}];

    // grab our backend URL from string resource
    std::string idsServerUrl(NV_API_URL_MAX_CHARS, '\0');
    if (LoadStringA(
        hInstance,
        IDS_STRING_SERVER_URL,
        idsServerUrl.data(),
        NV_API_URL_MAX_CHARS - 1
    ))
    {
        serverUrlTemplate = idsServerUrl;
    }
    else
    {
        // fallback to compiled-in value
        serverUrlTemplate = NV_API_URL_TEMPLATE;
    }

    spdlog::debug("serverUrlTemplate = {}", serverUrlTemplate);

    appPath = util::GetImageBasePathW();
    spdlog::debug("appPath = {}", appPath.string());

    appVersion = util::GetVersionFromFile(appPath);
    spdlog::debug("appVersion = {}", appVersion.to_string());

    appFilename = appPath.stem().string();
    spdlog::debug("appFilename = {}", appFilename);

    filenameRegex = NV_FILENAME_REGEX;
    spdlog::debug("filenameRegex = {}", filenameRegex);

    authority = Authority::Remote;
    spdlog::debug("authority = {}", magic_enum::enum_name(authority));

    //
    // Merge from config file, if available
    // 

    if (auto configFile = appPath.parent_path() / std::format("{}.json", appFilename); exists(configFile))
    {
        std::ifstream configFileStream(configFile);

        try
        {
            json data = json::parse(configFileStream);

            //
            // Override defaults, if specified
            // 

            serverUrlTemplate = data.value("/instance/serverUrlTemplate"_json_pointer, serverUrlTemplate);
            filenameRegex = data.value("/instance/filenameRegex"_json_pointer, filenameRegex);
            authority = data.value("/instance/authority"_json_pointer, authority);

            // populate shared config first either from JSON file or with built-in defaults
            if (data.contains("shared"))
            {
                shared = data["shared"].get<SharedConfig>();
            }
        }
        catch (...)
        {
            // invalid config, too bad
            spdlog::error("Couldn't deserialize contents of {}", configFile.string());
        }

        configFileStream.close();
    }
    else
    {
        spdlog::info("No local configuration found at {}", configFile.string());
    }

    //
    // File name extraction
    // 

    std::regex productRegex(filenameRegex, std::regex_constants::icase);
    auto matchesBegin = std::sregex_iterator(appFilename.begin(), appFilename.end(), productRegex);
    auto matchesEnd = std::sregex_iterator();

    if (matchesBegin != matchesEnd)
    {
        if (const std::smatch& match = *matchesBegin; match.size() == 3)
        {
            manufacturer = match[1];
            product = match[2];

            spdlog::info("Extracted manufacturer {} and product {} values", manufacturer, product);
        }
        else
        {
            spdlog::warn("Unexpected match size using regex {} on {}", filenameRegex, appFilename);
        }
    }
    else
    {
        spdlog::info("Regex {} didn't match anything on {}", filenameRegex, appFilename);
    }

    // first try to build "manufacturer/product" and use filename as 
    // fallback if extraction via regex didn't yield any results
    tenantSubPath = (!manufacturer.empty() && !product.empty())
                        ? std::format("{}/{}", manufacturer, product)
                        : appFilename;
    spdlog::debug("tenantSubPath = {}", tenantSubPath);

    updateRequestUrl = std::vformat(serverUrlTemplate, std::make_format_args(tenantSubPath));
    spdlog::debug("updateRequestUrl = {}", updateRequestUrl);
}

models::InstanceConfig::~InstanceConfig()
{
    RestClient::disable();
}

std::tuple<bool, std::string> models::InstanceConfig::IsInstalledVersionOutdated(bool& isOutdated)
{
    const auto& release = GetSelectedRelease();

    switch (shared.detectionMethod)
    {
    //
    // Detect product version via registry key and value
    // 
    case ProductVersionDetectionMethod::RegistryValue:
        {
            spdlog::debug("Running product detection via registry value");
            const auto& cfg = shared.GetRegistryValueConfig();
            HKEY hive = nullptr;

            switch (cfg.hive)
            {
            case RegistryHive::HKCU:
                hive = HKEY_CURRENT_USER;
                break;
            case RegistryHive::HKLM:
                hive = HKEY_LOCAL_MACHINE;
                break;
            case RegistryHive::HKCR:
                hive = HKEY_CLASSES_ROOT;
                break;
            case RegistryHive::Invalid:
                return std::make_tuple(false, "Invalid hive value");
            }

            const auto subKey = ConvertAnsiToWide(cfg.key);
            const auto valueName = ConvertAnsiToWide(cfg.value);

            winreg::RegKey key;

            if (const winreg::RegResult result = key.TryOpen(hive, subKey, KEY_READ); !result)
            {
                spdlog::error("Failed to open {}\\{} key", magic_enum::enum_name(cfg.hive), cfg.key);
                return std::make_tuple(false, "Failed to open registry key for reading");
            }

            const auto& resource = key.TryGetStringValue(valueName);

            if (!resource.IsValid())
            {
                spdlog::error("Failed to access value {}", cfg.value);
                return std::make_tuple(false, "Failed to read registry value");
            }

            const std::wstring value = resource.GetValue();

            try
            {
                const semver::version localVersion{ConvertWideToANSI(value)};

                isOutdated = release.GetSemVersion() > localVersion;
                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (...)
            {
                spdlog::error("Failed to convert value {} into SemVer", ConvertWideToANSI(value));
                return std::make_tuple(false, "String to SemVer conversion failed");
            }

            return std::make_tuple(true, "OK");
        }
    //
    // Detect product by comparing version resource
    // 
    case ProductVersionDetectionMethod::FileVersion:
        {
            spdlog::debug("Running product detection via file version");
            const auto& cfg = shared.GetFileVersionConfig();

            try
            {
                isOutdated = release.GetSemVersion() > util::GetVersionFromFile(cfg.path);
                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (...)
            {
                spdlog::error("Failed to get version resource from {}", cfg.path);
                return std::make_tuple(false, "Failed to read file version resource");
            }

            return std::make_tuple(true, "OK");
        }
    //
    // Detect product by comparing expected file sizes
    // 
    case ProductVersionDetectionMethod::FileSize:
        {
            spdlog::debug("Running product detection via file size");
            const auto& cfg = shared.GetFileSizeConfig();

            try
            {
                const std::filesystem::path file{cfg.path};

                isOutdated = file_size(file) != cfg.size;
                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (...)
            {
                spdlog::error("Failed to get file size from {}", cfg.path);
                return std::make_tuple(false, "Failed to read file size");
            }

            return std::make_tuple(true, "OK");
        }
    //
    // Detect product by hashing a given file checksum
    // 
    case ProductVersionDetectionMethod::FileChecksum:
        {
            spdlog::debug("Running product detection via file checksum");
            const auto& cfg = shared.GetFileChecksumConfig();

            if (!std::filesystem::exists(cfg.path))
            {
                spdlog::error("File {} doesn't exist", cfg.path);
                return std::make_tuple(false, "File to hash not found");
            }

            std::ifstream file(cfg.path, std::ios::binary);

            if (!file.is_open())
            {
                spdlog::error("Failed to open file {}", cfg.path);
                return std::make_tuple(false, "Failed to open file");
            }

            constexpr std::size_t chunkSize = 4 * 1024; // 4 KB

            switch (cfg.algorithm)
            {
            case ChecksumAlgorithm::MD5:
                {
                    spdlog::debug("Hashing with MD5");
                    MD5 alg;

                    std::vector<char> buffer(chunkSize);
                    while (!file.eof())
                    {
                        file.read(buffer.data(), buffer.size());
                        std::streamsize bytesRead = file.gcount();

                        alg.add(buffer.data(), bytesRead);

                        if (bytesRead < chunkSize && !file.eof())
                        {
                            if (file.fail())
                            {
                                spdlog::error("Failed to read file {} to end", cfg.path);
                                return std::make_tuple(false, "Failed to read file");
                            }
                        }
                    }

                    isOutdated = !util::icompare(alg.getHash(), cfg.hash);
                    spdlog::debug("isOutdated = {}", isOutdated);

                    return std::make_tuple(true, "OK");
                }
            case ChecksumAlgorithm::SHA1:
                {
                    spdlog::debug("Hashing with SHA1");
                    SHA1 alg;

                    std::vector<char> buffer(chunkSize);
                    while (!file.eof())
                    {
                        file.read(buffer.data(), buffer.size());
                        std::streamsize bytesRead = file.gcount();

                        alg.add(buffer.data(), bytesRead);

                        if (bytesRead < chunkSize && !file.eof())
                        {
                            if (file.fail())
                            {
                                spdlog::error("Failed to read file {} to end", cfg.path);
                                return std::make_tuple(false, "Failed to read file");
                            }
                        }
                    }

                    isOutdated = !util::icompare(alg.getHash(), cfg.hash);
                    spdlog::debug("isOutdated = {}", isOutdated);

                    return std::make_tuple(true, "OK");
                }
            case ChecksumAlgorithm::SHA256:
                {
                    spdlog::debug("Hashing with SHA256");
                    SHA256 alg;

                    std::vector<char> buffer(chunkSize);
                    while (!file.eof())
                    {
                        file.read(buffer.data(), buffer.size());
                        std::streamsize bytesRead = file.gcount();

                        alg.add(buffer.data(), bytesRead);

                        if (bytesRead < chunkSize && !file.eof())
                        {
                            if (file.fail())
                            {
                                spdlog::error("Failed to read file {} to end", cfg.path);
                                return std::make_tuple(false, "Failed to read file");
                            }
                        }
                    }

                    isOutdated = !util::icompare(alg.getHash(), cfg.hash);
                    spdlog::debug("isOutdated = {}", isOutdated);

                    return std::make_tuple(true, "OK");
                }
            case ChecksumAlgorithm::Invalid:
                spdlog::error("Invalid hashing algorithm specified");
                return std::make_tuple(false, "Invalid hashing algorithm");
            }

            file.close();

            break;
        }
    case ProductVersionDetectionMethod::Invalid:
        spdlog::error("Invalid detection method specified");
        return std::make_tuple(false, "Invalid detection method");
    }

    spdlog::error("No detection method matched");
    return std::make_tuple(false, "Unsupported detection method");
}

std::tuple<bool, std::string> models::InstanceConfig::RegisterAutostart(const std::string& launchArgs) const
{
    winreg::RegKey key;
    const auto subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        spdlog::error("Failed to open {}", ConvertWideToANSI(subKey));
        return std::make_tuple(false, "Failed to open registry key");
    }

    std::stringstream ss;
    ss << "\"" << appPath.string() << "\" " << launchArgs;

    if (const auto writeResult = key.TrySetStringValue(
            ConvertAnsiToWide(appFilename),
            ConvertAnsiToWide(ss.str()));
        !writeResult)
    {
        spdlog::error("Failed to write autostart value");
        return std::make_tuple(false, "Failed to write registry value");
    }

    return std::make_tuple(true, "OK");
}

std::tuple<bool, std::string> models::InstanceConfig::RemoveAutostart() const
{
    winreg::RegKey key;
    const auto subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        spdlog::error("Failed to open {}", ConvertWideToANSI(subKey));
        return std::make_tuple(false, "Failed to open registry key");
    }

    if (const auto result = key.TryDeleteValue(ConvertAnsiToWide(appFilename)); !result)
    {
        spdlog::error("Failed to delete autostart value");
        return std::make_tuple(false, "Failed to delete autostart value");
    }

    return std::make_tuple(true, "OK");
}
