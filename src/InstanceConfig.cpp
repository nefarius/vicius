#include "pch.h"
#include "Common.h"
#include "Util.h"
#include "InstanceConfig.hpp"
#include "NAuthenticode.h"

models::InstanceConfig::InstanceConfig(HINSTANCE hInstance, argh::parser& cmdl, PDWORD abortError)
    : appInstance(hInstance)
{
    //
    // Initialize everything in here that depends on CLI arguments, the environment and a potential configuration file
    //

#pragma region Logging

    //
    // Setup logger
    //

#if !defined(NV_FLAGS_NO_LOGGING)
    const auto logLevel = magic_enum::enum_cast<spdlog::level::level_enum>(cmdl(NV_CLI_PARAM_LOG_LEVEL).str());

    auto debugSink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
    debugSink->set_level(logLevel.has_value() ? logLevel.value() : spdlog::level::info);

    std::shared_ptr<spdlog::logger> logger;

    try
    {
        if (cmdl({NV_CLI_PARAM_LOG_TO_FILE}))
        {
            const auto logFilename = cmdl({NV_CLI_PARAM_LOG_TO_FILE}).str();
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilename, true);
            fileSink->set_level(logLevel.has_value() ? logLevel.value() : spdlog::level::info);

            logger = std::make_shared<spdlog::logger>(spdlog::logger(NV_LOGGER_NAME, {debugSink, fileSink}));
        }
        else
        {
            logger = std::make_shared<spdlog::logger>(NV_LOGGER_NAME, debugSink);
        }
    }
    catch (...)
    {
        // fallback action; user might have specified an invalid path
        logger = std::make_shared<spdlog::logger>(NV_LOGGER_NAME, debugSink);
    }

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
#endif

#pragma endregion

    this->overrideSuccessCode = static_cast<bool>(cmdl({NV_CLI_PARAM_OVERRIDE_OK}) >> this->overriddenSuccessCode);

    spdlog::debug("Initializing updater instance (PID: {})", GetCurrentProcessId());

    //
    // Defaults and embedded stuff
    //

    this->isSilent = cmdl[ {NV_CLI_BACKGROUND} ] || cmdl[ {NV_CLI_SILENT} ] || cmdl[ {NV_CLI_AUTOSTART} ]
                  || cmdl[ {NV_CLI_SILENT_UPDATE} ];
    this->ignorePostponePeriod = cmdl[ {NV_CLI_IGNORE_POSTPONE} ];

#if !defined(NV_FLAGS_NO_SERVER_URL_RESOURCE)
    // grab our backend URL from string resource
    std::string idsServerUrl(NV_API_URL_MAX_CHARS, '\0');
    if (LoadStringA(hInstance, IDS_STRING_SERVER_URL, idsServerUrl.data(), NV_API_URL_MAX_CHARS - 1))
    {
        serverUrlTemplate = idsServerUrl;
    }
    else
    {
#endif
    // fallback to compiled-in value
    this->serverUrlTemplate = NV_API_URL_TEMPLATE;
#if !defined(NV_FLAGS_NO_SERVER_URL_RESOURCE)
    }
#endif

#if !defined(NDEBUG)
    if (cmdl({NV_CLI_PARAM_SERVER_URL}))
    {
        this->serverUrlTemplate = cmdl(NV_CLI_PARAM_SERVER_URL).str();
    }
#endif

    spdlog::debug("serverUrlTemplate = {}", this->serverUrlTemplate);
    spdlog::debug("fallbackServerUrlTemplates.has_value() = {}", this->fallbackServerUrlTemplates.has_value());
    spdlog::debug("fallbackServerUrlTemplates.size() = {}",
                  this->fallbackServerUrlTemplates.has_value() ? this->fallbackServerUrlTemplates.value().size() : 0);

    // optional update channel
    if (cmdl({NV_CLI_PARAM_CHANNEL}))
    {
        this->channel = cmdl(NV_CLI_PARAM_CHANNEL).str();
    }

    spdlog::debug("channel = {}", channel);

    this->appPath = util::GetImageBasePathW();
    spdlog::debug("appPath = {}", appPath);

    //
    // Main module checks
    // 

    const std::regex renamingIssuePattern(R"((\.exe){2,}$)", std::regex_constants::icase);

    /*
     * Catches a common issue with renaming the file thanks to Windows
     * Explorers' idiotic default settings of hiding the file extension
     **/
    if (std::regex_match(this->appPath.string(), renamingIssuePattern))
    {
        spdlog::error("Mangled module name detected (appPath: {})", this->appPath.string());
        if (abortError)
        {
            *abortError = NV_E_INVALID_MODULE_NAME;
        }
        return;
    }

    if (cmdl({NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE}))
    {
        HANDLE appHandle{};
        // A HANDLE is a pointer; cast it to a non-pointer type to make the command line parser to treat the input as decimal
        cmdl(NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE) >> reinterpret_cast<uintptr_t&>(appHandle);
        if (!appHandle)
        {
            spdlog::error("No handle or 0 passed to {}", NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE);
        }
        else if (appHandle == GetCurrentProcess())
        {
            spdlog::error(
                "Pseudo-handle (e.g. from GetCurrentProcess()) passed to {}; use DuplicateHandle() with bInheritHandle set",
                NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE);
        }
        else if (!GetProcessId(appHandle))
        {
            spdlog::error(
                "Value {} passed to {} was not a valid HANDLE; use DuplicateHandle() with bInheritHandle set. If you "
                "passed a PID, use OpenProcess(). Error: {:#010x}",
                reinterpret_cast<uintptr_t>(appHandle),
                NV_CLI_PARAM_TERMINATE_PROCESS_BEFORE_UPDATE,
                static_cast<uint32_t>(GetLastError())
                );
        }
        else
        {
            this->terminateProcessBeforeUpdate = appHandle;
            spdlog::debug("Will terminate process with PID {} if there is an update", GetProcessId(appHandle));
        }
    }

#pragma region Temporary child process launch verification

    const auto parentProcessId = nefarius::winapi::GetParentProcessID(GetCurrentProcessId());
    spdlog::debug("parentProcessId = {}", parentProcessId.value_or(0));

    const auto parentPath = nefarius::winapi::GetProcessFullPath<std::string>(parentProcessId.value_or(0));

    //
    // Make sure our parent is originating form the exact same file to not load configuration from an impostor *sus*
    //
    if (cmdl[ {NV_CLI_TEMPORARY} ] && parentPath)
    {
        // those can not be used together
        if (this->isSilent)
        {
            spdlog::error("Temporary and silent switches can not be used together");
            if (abortError)
            {
                *abortError = NV_E_INVALID_PARAMETERS;
            }
            return;
        }

        parentAppPath = std::get<std::string>(parentPath.value());
        spdlog::debug("parentAppPath = {}", parentAppPath.value());

        std::ifstream parentAppFileStream(parentAppPath.value(), std::ios::binary);
        std::ifstream currentAppFileStream(appPath, std::ios::binary);

        if (parentAppFileStream.is_open() && currentAppFileStream.is_open())
        {
            SHA256 parentSha256Alg, currentSha256Alg;
            // improve hashing speed
            constexpr std::size_t chunkSize = 4 * 1024; // 4 KB

            std::vector<char> parentAppFileBuffer(chunkSize);
            while (!parentAppFileStream.eof())
            {
                parentAppFileStream.read(parentAppFileBuffer.data(), parentAppFileBuffer.size());
                std::streamsize bytesRead = parentAppFileStream.gcount();

                parentSha256Alg.add(parentAppFileBuffer.data(), bytesRead);

                if (bytesRead < chunkSize && !parentAppFileStream.eof())
                {
                    if (parentAppFileStream.fail())
                    {
                        spdlog::error("Failed to read file {} to the end for hashing, aborting", parentAppPath.value());
                        break;
                    }
                }
            }

            std::vector<char> currentAppFileBuffer(chunkSize);
            while (!currentAppFileStream.eof())
            {
                currentAppFileStream.read(currentAppFileBuffer.data(), currentAppFileBuffer.size());
                std::streamsize bytesRead = currentAppFileStream.gcount();

                currentSha256Alg.add(currentAppFileBuffer.data(), bytesRead);

                if (bytesRead < chunkSize && !currentAppFileStream.eof())
                {
                    if (currentAppFileStream.fail())
                    {
                        spdlog::error("Failed to read file {} to the end for hashing, aborting", appPath);
                        break;
                    }
                }
            }

            const auto hashLhs = parentSha256Alg.getHash();
            const auto hashRhs = currentSha256Alg.getHash();

            spdlog::debug("Hashes LHS {} vs. RHS {}", hashLhs, hashRhs);

            // trusted parent, we can continue
            this->isTemporaryCopy = util::icompare(hashLhs, hashRhs);

            if (!this->isTemporaryCopy)
            {
                spdlog::error("Parent SHA256 {} didn't match current SHA256 {}, will not continue with temporary copy", hashLhs,
                              hashRhs);

                this->TryDisplayErrorDialog("Updater process module hash mismatch",
                                            "The module integrity check has failed, "
                                            "temporary child process will not be spawned for security reasons.");
            }
        }
        else
        {
            spdlog::warn("Failed to open process module streams for comparison");
            // prerequisites for running in this mode not met
            this->isTemporaryCopy = false;
        }

        parentAppFileStream.close();
        currentAppFileStream.close();
    }

    spdlog::debug("isTemporaryCopy = {}", this->isTemporaryCopy);

#pragma endregion

    try
    {
        const auto fileVersion = util::GetVersionFromFile(appPath);
        if (const std::string suffix = NV_CUSTOM_VERSION_SUFFIX; suffix.empty())
        {
            // Keeps the 4th (revision) segment that GetVersionFromFile stashes as build metadata.
            appVersion = fileVersion;
        }
        else
        {
            // Custom suffix is a SemVer pre-release or build tail on the numeric core.
            // Keep the Win32 revision (already in build metadata) so distinct builds
            // do not compare equal after suffixing.
            const std::string& revision = fileVersion.build_meta();
            std::string suffixed;
            if (suffix.starts_with("+"))
            {
                suffixed = revision.empty()
                             ? std::format("{}.{}.{}{}", fileVersion.major(), fileVersion.minor(), fileVersion.patch(),
                                           suffix)
                             : std::format("{}.{}.{}+{}.{}", fileVersion.major(), fileVersion.minor(), fileVersion.patch(),
                                           revision, suffix.substr(1));
            }
            else
            {
                suffixed = std::format("{}.{}.{}{}", fileVersion.major(), fileVersion.minor(), fileVersion.patch(), suffix);
                if (!revision.empty())
                {
                    suffixed += std::format("+{}", revision);
                }
            }
            util::toSemVerCompatible(suffixed);
            appVersion = semver::version::parse(suffixed);
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to determine app version from {}: {}", appPath.string(), e.what());
        appVersion = semver::version{0, 0, 0};
    }
    spdlog::debug("appVersion = {}", appVersion.str());

    appFilename = appPath.stem().string();
    spdlog::debug("appFilename = {}", appFilename);

    filenameRegex = NV_FILENAME_REGEX;
    spdlog::debug("filenameRegex = {}", filenameRegex);

    authority = Authority::Remote;
    spdlog::debug("authority = {}", magic_enum::enum_name(authority));

    // parse optional additional HTTP headers
    for (auto& param : cmdl.params("add-header"))
    {
        std::string kvp = param.second;
        std::ranges::replace(kvp, '=', ' ');
        std::stringstream keyValue(kvp);

        std::string name, value;
        keyValue >> name;
        keyValue >> value;

        additionalHeaders.insert(std::make_pair(name, value));
    }

    //
    // Capture the updater's own Authenticode signature (used for FromUpdaterBinary publisher pinning).
    // We do NOT pass WTD_LIFETIME_SIGNING_FLAG so that time-stamped certs remain valid after expiry.
    //
    if (!NVerifyFileSignature(appPath.wstring().c_str(), &appSigInfo))
    {
        switch (appSigInfo.lValidationResult)
        {
            case TRUST_E_NOSIGNATURE:
                spdlog::info("Updater executable {} is not signed (FromUpdaterBinary pinning unavailable)",
                             appPath.string());
                break;
            default:
                spdlog::warn("Updater executable {} signature validation returned {:#x}",
                             appPath.string(), static_cast<unsigned long>(appSigInfo.lValidationResult));
                break;
        }
        isUpdaterSigned = false;
    }
    else
    {
        // Extract extended cert info for subjectName comparison
        if (crypto::ExtractSignatureInformation(appPath.wstring().c_str(), &appCertInfo))
        {
            isUpdaterSigned = true;
            spdlog::info("Updater executable is signed; subject='{}'",
                         appCertInfo.SubjectName
                             ? ConvertWideToANSI(std::wstring(appCertInfo.SubjectName))
                             : "(unknown)");
        }
        else
        {
            spdlog::warn("Updater is signed but cert info could not be extracted");
            isUpdaterSigned = false;
        }
    }

    //
    // Merge from config file, if available
    //

#if !defined(NV_FLAGS_NO_CONFIG_FILE)
    const auto configFileName = std::format("{}.json", appFilename);
    // ReSharper disable once CppTooWideScopeInitStatement
    auto configFile = (!isTemporaryCopy || !parentAppPath.has_value())
                          ? appPath.parent_path() / configFileName
                          : parentAppPath.value().parent_path() / configFileName;

    if (exists(configFile))
    {
        std::ifstream configFileStream(configFile);

        try
        {
            json data = json::parse(configFileStream);

            //
            // Override defaults, if specified
            //

            serverUrlTemplate = data.value("/instance/serverUrlTemplate"_json_pointer, serverUrlTemplate);
            if (data.contains("/instance/fallbackServerUrlTemplates"_json_pointer))
            {
                fallbackServerUrlTemplates = data.at("/instance/fallbackServerUrlTemplates"_json_pointer).get<std::vector<std::string>>();
            }
            filenameRegex = data.value("/instance/filenameRegex"_json_pointer, filenameRegex);
            authority = data.value("/instance/authority"_json_pointer, authority);

            // CLI arg takes priority
            if (channel.empty())
            {
                channel = data.value("/instance/channel"_json_pointer, channel);
            }

            // populate shared config first either from JSON file or with built-in defaults
            if (data.contains("shared"))
            {
                merged = data[ "shared" ].get<MergedConfig>();
            }
        }
        catch (...)
        {
            // invalid config, too bad
            spdlog::error("Couldn't deserialize contents of {}", configFile);
        }

        configFileStream.close();
    }
    else
    {
        spdlog::info("No local configuration found at {}", configFile);
    }
#endif

    this->forceLocalVersion = static_cast<bool>(cmdl({NV_CLI_PARAM_FORCE_LOCAL_VERSION}));

    // Strict verification: checksum required, Authenticode required, publisher pin must match
    this->strictVerification = static_cast<bool>(cmdl[{NV_CLI_STRICT_VERIFICATION}]);
    if (this->strictVerification)
    {
        spdlog::info("Strict verification mode is ACTIVE");
        // Strict mode implies Required signature verification mode if not already set higher
        if (merged.signatureVerificationMode == SignatureVerificationMode::WhenPresent ||
            merged.signatureVerificationMode == SignatureVerificationMode::Disabled)
        {
            merged.signatureVerificationMode = SignatureVerificationMode::Required;
        }
        if (merged.signaturePolicy == SignatureComparisonPolicy::Relaxed)
        {
            merged.signaturePolicy = SignatureComparisonPolicy::Strict;
        }
    }

    if (this->forceLocalVersion)
    {
        // Takes priority over config file; intended for manual debugging/fixing stuff
        merged.detectionMethod = ProductVersionDetectionMethod::FixedVersion;
        merged.detection = FixedVersionConfig{cmdl(NV_CLI_PARAM_FORCE_LOCAL_VERSION).str()};
    }
    else if (cmdl({NV_CLI_PARAM_LOCAL_VERSION}) && merged.detectionMethod == ProductVersionDetectionMethod::Invalid)
    {
        // Config files take priority; intended for use when launched by an application
        merged.detectionMethod = ProductVersionDetectionMethod::FixedVersion;
        merged.detection = FixedVersionConfig{cmdl(NV_CLI_PARAM_LOCAL_VERSION).str()};
    }

    // avoid accidental fork bomb :P
    if (this->isTemporaryCopy) this->merged.runAsTemporaryCopy = false;

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
            manufacturer = match[ 1 ];
            product = match[ 2 ];

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

    // first try to build "manufacturer/product", then "manufacturer/product/channel" and
    // then use filename as fallback if extraction via regex didn't yield any results
    tenantSubPath = (!manufacturer.empty() && !product.empty())
                        ? channel.empty()
                              ? std::format("{}/{}", manufacturer, product)
                              : std::format("{}/{}/{}", manufacturer, product, channel)
                        : channel.empty()
                        ? appFilename
                        : std::format("{}/{}", channel, appFilename);
    spdlog::debug("tenantSubPath = {}", tenantSubPath);

    updateRequestUrl = std::vformat(serverUrlTemplate, std::make_format_args(tenantSubPath));
    spdlog::debug("updateRequestUrl = {}", updateRequestUrl);

    fallbackUpdateRequestUrls.clear();
    if (fallbackServerUrlTemplates.has_value())
    {
        fallbackUpdateRequestUrls.reserve(fallbackServerUrlTemplates.value().size());

        for (const auto& tpl : fallbackServerUrlTemplates.value())
        {
            try
            {
                fallbackUpdateRequestUrls.push_back(std::vformat(tpl, std::make_format_args(tenantSubPath)));
            }
            catch (const std::exception& e)
            {
                spdlog::error("Failed to render fallback URL template '{}', error: {}", tpl, e.what());
            }
        }
    }
}

models::InstanceConfig::~InstanceConfig()
{
    if (appSigInfo.lValidationResult == ERROR_SUCCESS)
    {
        NCertFreeSigInfo(&appSigInfo);
    }

    if (isUpdaterSigned)
    {
        crypto::FreeSignatureInformation(&appCertInfo);
    }

    // Ensure no in-flight download uses libcurl while we tear down global state.
    RequestAbortDownload();
    WaitForDownloadToFinish();

    // clean up release resources
    for (const auto& release : remote.releases)
    {
        if (release.localTempFilePath.empty())
        {
            continue;
        }

        // delete local setup copy
        if (DeleteFileA(release.localTempFilePath.string().c_str()) == FALSE)
        {
            spdlog::warn("Failed to delete temporary file {}, error {:#x}, message {}", release.localTempFilePath,
                         GetLastError(), winapi::GetLastErrorStdStr());

            // try to get rid of it on next reboot
            MoveFileExA(release.localTempFilePath.string().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }

    // make sure everything ends up on file before shutting down
    spdlog::default_logger()->flush();
}

std::expected<bool, std::string> models::InstanceConfig::IsInstalledVersionOutdated()
{
    if (remote.releases.empty())
    {
        spdlog::error("Server provided no releases, no data to compare to");
        return std::unexpected("Server provided no releases, no data to compare to");
    }

    const auto& release = GetSelectedRelease();

    switch (merged.detectionMethod)
    {
        //
        // We have a fixed version, either from command line, or - presumably local - config file.
        //
        case ProductVersionDetectionMethod::FixedVersion:
        {
            auto asString = merged.GetFixedVersionConfig().version;
            spdlog::debug("Using fixed product version {}", asString);
            try
            {
                util::toSemVerCompatible(asString);
                const semver::version localVersion = semver::version::parse(asString);

                const bool isOutdated =
                    util::CompareVersions(release.GetDetectionSemVersion().value_or(localVersion), localVersion) > 0;
                spdlog::debug("isOutdated = {}", isOutdated);
                return isOutdated;
            }
            catch (const std::exception& e)
            {
                // The version string is present but unparseable. Treat the local install as
                // outdated so the server's trusted update can proceed rather than hard-failing.
                spdlog::warn("Cannot parse local version '{}' as SemVer ({}); treating as outdated",
                             asString, e.what());
                return true;
            }
        }

        //
        // Detect product version via registry key and value
        //
        case ProductVersionDetectionMethod::RegistryValue:
        {
            spdlog::debug("Running product detection via registry value");
            const auto cfg = merged.GetRegistryValueConfig();
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
                    return std::unexpected("Invalid hive value");
            }

            const auto subKey = ConvertAnsiToWide(cfg.key);
            const auto valueName = ConvertAnsiToWide(cfg.value);

            winreg::RegKey key;
            REGSAM flags = KEY_READ;

            if (cfg.view > RegistryView::Default)
            {
                flags |= static_cast<REGSAM>(cfg.view);
            }

            if (const winreg::RegResult result = key.TryOpen(hive, subKey, flags); !result)
            {
                spdlog::error("Failed to open {}\\{} key", magic_enum::enum_name(cfg.hive), cfg.key);
                return std::unexpected("Failed to open registry key for reading");
            }

            const auto resource = key.TryGetStringValue(valueName);

            if (!resource.IsValid())
            {
                spdlog::error("Failed to access value {}", cfg.value);
                return std::unexpected("Failed to read registry value");
            }

            const std::wstring value = resource.GetValue();

            bool isOutdated = false;
            try
            {
                std::string nVersion = ConvertWideToANSI(value);
                util::toSemVerCompatible(nVersion);
                const semver::version localVersion = semver::version::parse(nVersion);

                isOutdated =
                    util::CompareVersions(release.GetDetectionSemVersion().value_or(localVersion), localVersion) > 0;
                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (const std::exception& e)
            {
                // The registry value is present but its content is not a parseable version string.
                // Treat the local install as outdated so the server's trusted update can proceed.
                spdlog::warn("Cannot parse registry version '{}' as SemVer ({}); treating as outdated",
                             ConvertWideToANSI(value), e.what());
                return true;
            }

            return isOutdated;
        }
        //
        // Detect product by comparing version resource
        //
        case ProductVersionDetectionMethod::FileVersion:
        {
            spdlog::debug("Running product detection via file version");
            const auto cfg = merged.GetFileVersionConfig();
            const auto filePath = RenderInjaTemplate(cfg.input, cfg.data);

            if (!std::filesystem::exists(filePath))
            {
                spdlog::error("File {} doesn't exist", filePath);
                return std::unexpected("Product detection file not found");
            }

            if (cfg.statement == VersionResource::Invalid)
            {
                spdlog::error("Invalid resource statement provided");
                return std::unexpected("Invalid resource statement provided");
            }

            bool isOutdated = false;
            try
            {
                switch (cfg.statement)
                {
                    case VersionResource::FILEVERSION:
                    {
                        const auto fileVer = winapi::GetWin32ResourceFileVersion(filePath);
                        isOutdated =
                            util::CompareVersions(release.GetDetectionSemVersion().value_or(fileVer), fileVer) > 0;
                        break;
                    }
                    case VersionResource::PRODUCTVERSION:
                    {
                        const auto prodVer = winapi::GetWin32ResourceProductVersion(filePath);
                        isOutdated =
                            util::CompareVersions(release.GetDetectionSemVersion().value_or(prodVer), prodVer) > 0;
                        break;
                    }
                    case VersionResource::Invalid:
                        spdlog::warn("Unexpected version resource statement");
                        isOutdated = true;
                        break;
                }

                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (...)
            {
                // The file exists but its version resource could not be read or compared.
                // Treat as outdated so the update can proceed rather than hard-failing.
                spdlog::warn("Failed to read version resource from {}; treating as outdated", filePath);
                return true;
            }

            return isOutdated;
        }
        //
        // Detect product by comparing expected file sizes
        //
        case ProductVersionDetectionMethod::FileSize:
        {
            spdlog::debug("Running product detection via file size");
            const auto cfg = merged.GetFileSizeConfig();

            if (!release.detectionSize.has_value())
            {
                spdlog::error("Selected release is missing size data");
                return std::unexpected("Selected release is missing size data");
            }

            bool isOutdated = false;
            try
            {
                const auto filePath = RenderInjaTemplate(cfg.input, cfg.data);

                if (!std::filesystem::exists(filePath))
                {
                    spdlog::error("File {} doesn't exist", filePath);
                    return std::unexpected("Product detection file not found");
                }

                const std::filesystem::path file{filePath};

                isOutdated = file_size(file) != release.detectionSize.value();
                spdlog::debug("isOutdated = {}", isOutdated);
            }
            catch (...)
            {
                spdlog::error("Failed to get file size from {}", cfg.input);
                return std::unexpected("Failed to read file size");
            }

            return isOutdated;
        }
        //
        // Detect product by hashing a given file checksum
        //
        case ProductVersionDetectionMethod::FileChecksum:
        {
            spdlog::debug("Running product detection via file checksum");
            const auto cfg = merged.GetFileChecksumConfig();
            const auto filePath = RenderInjaTemplate(cfg.input, cfg.data);

            if (!release.detectionChecksum.has_value())
            {
                spdlog::error("Selected release is missing checksum data");
                return std::unexpected("Selected release is missing checksum data");
            }

            if (!std::filesystem::exists(filePath))
            {
                spdlog::error("File {} doesn't exist", filePath);
                return std::unexpected("File to hash not found");
            }

            std::ifstream file(filePath, std::ios::binary);

            if (!file.is_open())
            {
                spdlog::error("Failed to open file {}", filePath);
                return std::unexpected("Failed to open file");
            }

            // improve hashing speed
            constexpr std::size_t chunkSize = 4 * 1024; // 4 KB

            // checksum detection data of release
            const auto& hashCfg = release.detectionChecksum.value();

            switch (hashCfg.checksumAlg)
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
                                spdlog::error("Failed to read file {} to end", filePath);
                                return std::unexpected("Failed to read file");
                            }
                        }
                    }

                    const bool isOutdated = !util::icompare(alg.getHash(), hashCfg.checksum);
                    spdlog::debug("isOutdated = {}", isOutdated);
                    return isOutdated;
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
                                spdlog::error("Failed to read file {} to end", filePath);
                                return std::unexpected("Failed to read file");
                            }
                        }
                    }

                    const bool isOutdated = !util::icompare(alg.getHash(), hashCfg.checksum);
                    spdlog::debug("isOutdated = {}", isOutdated);
                    return isOutdated;
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
                                spdlog::error("Failed to read file {} to end", filePath);
                                return std::unexpected("Failed to read file");
                            }
                        }
                    }

                    const bool isOutdated = !util::icompare(alg.getHash(), hashCfg.checksum);
                    spdlog::debug("isOutdated = {}", isOutdated);
                    return isOutdated;
                }
                case ChecksumAlgorithm::Invalid:
                    spdlog::error("Invalid hashing algorithm specified");
                    return std::unexpected("Invalid hashing algorithm");
            }

            break;
        }
        //
        // Evaluate custom expression
        //
        case ProductVersionDetectionMethod::CustomExpression:
        {
            spdlog::debug("Running product detection via custom expression");
            const auto cfg = merged.GetCustomExpressionConfig();

            if (cfg.input.empty())
            {
                spdlog::error("Custom expression is missing inja template");
                return std::unexpected("Custom expression is missing inja template");
            }

            json data;
            // provides local state to parser
            data[ "merged" ] = merged;
            data[ "remote" ] = remote;
            // provides user-supplied data under fixed variable name
            data[ "parameters" ] = cfg.data;

            try
            {
                // expected return value of "true" for outdated and "false" for up2date
                const auto result = RenderInjaTemplate(cfg.input, data);

                // string to boolean conversion — fail explicitly on invalid output
                bool isOutdated = false;
                std::istringstream iss(result);
                if (!(iss >> std::boolalpha >> isOutdated))
                {
                    spdlog::error("Custom expression returned non-boolean value: '{}'", result);
                    return std::unexpected(std::format("Custom expression returned non-boolean value: '{}'", result));
                }
                spdlog::debug("isOutdated = {}", isOutdated);

                return isOutdated;
            }
            catch (const std::exception& ex)
            {
                spdlog::error("Parsing custom expression failed, error: {}", ex.what());
                return std::unexpected("Parsing custom expression failed");
            }
        }
        case ProductVersionDetectionMethod::Invalid:
            spdlog::error("Invalid detection method specified");
            return std::unexpected("Invalid detection method");
    }

    spdlog::error("No detection method matched");
    return std::unexpected("Unsupported detection method");
}

std::expected<void, std::string> models::InstanceConfig::RegisterAutostart(const std::string& launchArgs) const
{
    winreg::RegKey key;
    const auto subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        spdlog::error("Failed to open {} (code: {}, message: {})",
                      ConvertWideToANSI(subKey), result.Code(), ConvertWideToANSI(result.ErrorMessage()));
        return std::unexpected("Failed to open registry key");
    }

    std::stringstream ss;
    ss << "\"" << appPath.string() << "\" " << launchArgs;
    spdlog::debug("String value: {}", ss.str());

    if (const auto writeResult = key.TrySetStringValue(ConvertAnsiToWide(appFilename), ConvertAnsiToWide(ss.str())); !writeResult)
    {
        spdlog::error("Failed to write autostart value (code: {}, message: {})",
                      writeResult.Code(), ConvertWideToANSI(writeResult.ErrorMessage()));
        return std::unexpected("Failed to write registry value");
    }

    return {};
}

std::expected<void, std::string> models::InstanceConfig::RemoveAutostart() const
{
    winreg::RegKey key;
    const auto subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
    {
        spdlog::error("Failed to open {}", ConvertWideToANSI(subKey));
        return std::unexpected("Failed to open registry key");
    }

    if (const auto result = key.TryDeleteValue(ConvertAnsiToWide(appFilename)); !result)
    {
        spdlog::error("Failed to delete autostart value");
        return std::unexpected("Failed to delete autostart value");
    }

    return {};
}

void models::InstanceConfig::LaunchEmergencySite() const
{
    if (!this->remote.instance.has_value()) return;

    if (!this->remote.instance.value().emergencyUrl.has_value()) return;

    ShellExecuteA(nullptr, "open", this->remote.instance.value().emergencyUrl.value().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool models::InstanceConfig::TryRunTemporaryProcess() const
{
    if (!this->merged.runAsTemporaryCopy || this->isTemporaryCopy) return false;

    const auto userTempPath = winapi::GetUserTemporaryDirectory();
    if (!userTempPath) return false;

    std::filesystem::path temporaryUpdaterPath =
        std::filesystem::path(*userTempPath) / std::filesystem::path(std::format("{}.exe", this->appFilename));

    if (CopyFileA(this->appPath.string().c_str(), temporaryUpdaterPath.string().c_str(), FALSE) == FALSE) return false;

    // ensure cleanup at least on next reboot
    MoveFileExA(temporaryUpdaterPath.string().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    std::vector<std::string> narrow;

    narrow.reserve(__argc);
    for (int i = 0; i < __argc; i++)
    {
        narrow.push_back(__argv[ i ]); // NOLINT(modernize-use-emplace)
    }

    // throw away process path
    narrow.erase(narrow.begin());

    // slice together new launch arguments
    const std::string cliLine = narrow.empty()
                                    ? "--temporary {}"
                                    : std::format("--temporary {}",
                                                  std::accumulate(std::next(narrow.begin()),
                                                                  narrow.end(),
                                                                  narrow[ 0 ],
                                                                  [](const std::string& lhs, const std::string& rhs)
                                                                  {
                                                                      return std::format("{} {}", lhs, rhs);
                                                                  }));

    STARTUPINFOA info = {};
    info.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION updateProcessInfo = {};

    // re-launch temporary copy with additional "--temporary" flag
    if (!CreateProcessA(temporaryUpdaterPath.string().c_str(),
                        const_cast<LPSTR>(cliLine.c_str()),
                        nullptr,
                        nullptr,
                        TRUE,
                        0,
                        nullptr,
                        nullptr,
                        &info,
                        &updateProcessInfo))
    {
        DWORD win32Error = GetLastError();

        spdlog::error("Failed to launch {}, error {:#x}, message {}", temporaryUpdaterPath, win32Error,
                      winapi::GetLastErrorStdStr());

        return false;
    }

    return true;
}
