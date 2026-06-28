#pragma once
#include <restclient-cpp/connection.h>
#include <curl/curl.h>
#include <atomic>

#include "UpdateResponse.hpp"
#include "MergedConfig.hpp"
#include "NAuthenticode.h"

using json = nlohmann::json;

struct zip;
typedef struct zip zip_t;

namespace models
{
    /**
     * \brief If hitting duplicate instructions, which configuration gets the priority.
     */
    enum class Authority
    {
        ///< The local config file (if any) gets priority
        Local,
        ///< The server-side instructions get priority
        Remote,
        Invalid = -1
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Authority,
                                 {
                                   {Authority::Invalid, nullptr},
                                   {Authority::Local, magic_enum::enum_name(Authority::Local)},
                                   {Authority::Remote, magic_enum::enum_name(Authority::Remote)},
                                 })

    /**
     * \brief Local configuration file model.
     */
    /** Holds the outcome of a completed setup process. */
    struct SetupResult
    {
        DWORD exitCode{};
        DWORD win32Error{};
    };

    class InstanceConfig
    {
        /** The application instance handle */
        HINSTANCE appInstance{};
        /** Handle of the main SFML window */
        HWND windowHandle{};
        /** Full pathname of the updater process file */
        std::filesystem::path appPath;
        /** The updater application version */
        semver::version appVersion;
        /** Filename of the updater file without extension */
        std::string appFilename;
        /** The manufacturer name */
        std::string manufacturer;
        /** The product name */
        std::string product;
        /** The tenant sub-path */
        std::string tenantSubPath;
        /** URL of the update request */
        std::string updateRequestUrl;
        /** Optional pre-rendered fallback URLs for the update request */
        std::vector<std::string> fallbackUpdateRequestUrls;
        /** Full pathname of the updater parent process file, if any */
        std::optional<std::filesystem::path> parentAppPath;
        /** Process to terminate before update */
        std::optional<HANDLE> terminateProcessBeforeUpdate;

        /** The local and remote shared configuration */
        MergedConfig merged;
        /** The remote API response */
        UpdateResponse remote;

        /** The download task */
        std::optional<std::shared_future<std::expected<int, std::string>>> downloadTask;
        /** The setup task */
        std::optional<std::shared_future<std::expected<SetupResult, std::string>>> setupTask;
        /** The selected release numeric ID */
        int selectedRelease{0};
        /** Human-readable download failure reason from last attempt (if any) */
        std::string lastDownloadError{};
        /** True if the current download should abort ASAP */
        std::atomic_bool abortDownloadRequested{false};
        /** True if we run in any of the silent scenarios, false if not */
        bool isSilent{false};
        /** True if user chose to ignore postpone period */
        bool ignorePostponePeriod{false};
        /** True if this process is a temporary copy, false if not */
        bool isTemporaryCopy{false};
        /** True if NV_CLI_PARAM_FORCE_LOCAL_VERSION was specified, false if not */
        bool forceLocalVersion{false};
        /** Whether NV_CLI_PARAM_OVERRIDE_OK was supplied */
        bool overrideSuccessCode{false};
        /** The value supplied by NV_CLI_PARAM_OVERRIDE_OK */
        int overriddenSuccessCode{ERROR_SUCCESS};
        /** True if --strict-verification was passed or enabled via config */
        bool strictVerification{false};

        /** WinTrust result for the updater exe itself (populated at startup) */
        NSIGINFO appSigInfo{};
        /** Extended cert info for the updater exe (populated at startup when signed) */
        crypto::SIGNATURE_INFORMATION appCertInfo{};
        /** True if the updater exe itself carries a valid Authenticode signature */
        bool isUpdaterSigned{false};

        std::expected<int, std::string> DownloadRelease(curl_progress_callback progressFn, int releaseIndex);

        void SetCommonHeaders(_Inout_ std::unique_ptr<RestClient::Connection>& conn) const;

        std::expected<SetupResult, std::string> ExecuteSetup(const std::stop_token&);

    public:
        static constexpr int MAX_TIMEOUT_SECS = 180; // 3 minutes
        static constexpr int MAX_TIMEOUT_SECS_TOTAL = 3600; // 1 hour
        static constexpr int MAX_RETRY_COUNT = 10;
        static constexpr int MAX_REDIRECTS = 5;

        std::string serverUrlTemplate;
        std::optional<std::vector<std::string>> fallbackServerUrlTemplates;
        std::string filenameRegex;
        Authority authority;
        std::string channel;
        std::map<std::string, std::string> additionalHeaders;

        InstanceConfig() : authority(Authority::Remote) { }

        InstanceConfig(const InstanceConfig&) = delete;
        InstanceConfig(InstanceConfig&&) = delete;
        InstanceConfig& operator=(const InstanceConfig&) = delete;
        InstanceConfig& operator=(InstanceConfig&&) = delete;

        InstanceConfig(HINSTANCE hInstance, argh::parser& cmdl, PDWORD abortError);

        ~InstanceConfig();

        void SetWindowHandle(_In_ const HWND hWnd) { windowHandle = hWnd; }

        std::filesystem::path GetAppPath() const { return appPath; }
        semver::version GetAppVersion() const { return appVersion; }
        std::string GetAppFilename() const { return appFilename; }

        std::string GetWindowTitle() const { return merged.windowTitle; }
        std::string GetProductName() const { return merged.productName; }
        bool IsRemindButtonHidden() const { return merged.hideRemindButton; }

        bool SetSelectedRelease(const int releaseIndex = 0)
        {
            if (releaseIndex < 0 || releaseIndex >= static_cast<int>(remote.releases.size()))
            {
                return false;
            }

            selectedRelease = releaseIndex;

            return true;
        }

        std::filesystem::path GetLocalReleaseTempFilePath(const int releaseId = 0) const
        {
            return remote.releases[ releaseId ].localTempFilePath;
        }

        UpdateRelease& GetSelectedRelease() { return remote.releases[ selectedRelease ]; }

        int GetSelectedReleaseId() const { return selectedRelease; }

        /**
         * \brief Returns the most recent download failure reason, if any.
         */
        [[nodiscard]] std::string GetLastDownloadError() const { return lastDownloadError; }

        void SetLastDownloadError(const std::string& error) { lastDownloadError = error; }

        /**
         * \brief Requests the update configuration from the remote server.
         * \return True on success, false otherwise.
         */
        [[nodiscard]] std::expected<void, std::string> RequestUpdateInfo();

        /**
         * \brief Checks if a newer release than the local version is available.
         * \param currentVersion The local product version to check against.
         * \return True if a newer version is available, false otherwise.
         */
        [[nodiscard]] bool IsProductUpdateAvailable(const semver::version& currentVersion)
        {
            if (remote.releases.empty())
            {
                return false;
            }

            const auto latest = GetSelectedRelease().GetSemVersion();

            return latest > currentVersion;
        }

        /**
         * \brief Checks if a newer updater than the local version is available.
         * \return True if a newer version is available, false otherwise.
         */
        [[nodiscard]] bool IsNewerUpdaterAvailable() const
        {
            if (!remote.instance.has_value())
            {
                return false;
            }

            if (remote.instance.value().latestVersion.has_value() && remote.instance.value().latestUrl.has_value())
            {
                const auto latest = remote.instance.value().GetLatestUpdaterSemVersion();

                return latest > appVersion;
            }

            return false;
        }

        /**
         * \brief Checks if we have one single update release.
         * \return True if single update release, false otherwise.
         */
        _Must_inspect_result_ [[nodiscard]] bool HasSingleRelease() const { return remote.releases.size() == 1; }

        /**
         * \brief Checks if we have multiple update releases.
         * \return True if multiple update releases, false otherwise.
         */
        _Must_inspect_result_ [[nodiscard]] bool HasMultipleReleases() const { return remote.releases.size() > 1; }

        /**
         * \brief Starts the update release download.
         * \param releaseIndex Zero-based release index.
         * \param progressFn The download progress callback.
         */
        bool DownloadReleaseAsync(int releaseIndex, curl_progress_callback progressFn);


        /**
        * \brief Extract the zip file containing updated executables.
        *
        * \param zip the zip file; the caller retains ownership.
        * \return Path of folder containing extracted zip file contents on success; unexpected error string on failure.
        */
        [[nodiscard]] std::expected<std::filesystem::path, std::string> ExtractReleaseZip(zip_t* zip) const;

        /** Returned by GetReleaseDownloadStatus when a download task has been invoked. */
        struct DownloadStatus
        {
            bool isDownloading{};
            bool hasFinished{};
            /** Present when hasFinished is true (task produced a terminal result). The inner expected
             *  holds HTTP 200 on success or an error string on failure — a present optional does NOT
             *  imply the operation succeeded; check result->has_value() for that. */
            std::optional<std::expected<int, std::string>> result{};
        };

        /**
         * \brief Checks the current download status.
         * \return nullopt if no download was invoked; otherwise a DownloadStatus snapshot.
         */
        [[nodiscard]] std::optional<DownloadStatus> GetReleaseDownloadStatus() const;

        /**
         * \brief Reset the download async task state.
         */
        void ResetReleaseDownloadState();

        /**
         * \brief Requests the running download (if any) to abort as soon as possible.
         */
        void RequestAbortDownload();

        /**
         * \brief Waits for the running download (if any) to finish.
         */
        void WaitForDownloadToFinish();

        /**
         * \brief Checks the version of the installed product against the latest available release.
         * \return True (outdated) or false (up-to-date) on success; unexpected error string on failure.
         */
        [[nodiscard]] std::expected<bool, std::string> IsInstalledVersionOutdated();

        [[nodiscard]] std::expected<void, std::string> CreateScheduledTask(const std::string& launchArgs = NV_CLI_BACKGROUND) const;

        [[nodiscard]] std::expected<void, std::string> RemoveScheduledTask() const;

        /**
         * \brief Registers the updater executable in current users' autostart.
         * \param launchArgs Optional launch arguments when run at autostart.
         * \return Empty on success; unexpected error string on failure.
         */
        [[nodiscard]] std::expected<void, std::string> RegisterAutostart(const std::string& launchArgs = NV_CLI_AUTOSTART) const;

        /**
         * \brief Removes the autostart entry for the current user.
         * \return Empty on success; unexpected error string on failure.
         */
        [[nodiscard]] std::expected<void, std::string> RemoveAutostart() const;

        /**
         * \brief Extracts the embedded self-updater DLL resource into our own app as ADS.
         * \return Empty on success; unexpected error string on failure.
         */
        [[nodiscard]] std::expected<void, std::string> ExtractSelfUpdater() const;

        /**
         * \brief Checks if the current app working directory can be written to.
         * \return True if we can write, false otherwise.
         */
        bool HasWritePermissions() const;

        /**
         * \brief Attempts to spawn the self-updater component.
         * \return Empty on success (end this process if the case); unexpected error string on failure.
         */
        [[nodiscard]] std::expected<void, std::string> RunSelfUpdater() const;

        /**
         * \brief Attempts to bing up the up-to-date dialog.
         * \param force True to force despite silent flags being present.
         */
        void TryDisplayUpToDateDialog(bool force = false) const;

        /**
         * \brief Attempts to bring up a Task Dialog with error information.
         * \param header The header text of the Task Dialog.
         * \param body The body text of the Task Dialog.
         * \param force True to ignore silent flags.
         */
        void TryDisplayErrorDialog(const std::string& header, const std::string& body, bool force = false) const;

        /**
         * \brief Queries whether we're run as background or silent.
         * \return True if silent/background requested.
         */
        [[nodiscard]] bool IsSilent() const { return isSilent; }

        /**
         * \brief Checks whether an emergency URL is specified by the server.
         * \return True if URL is set, false otherwise.
         */
        [[nodiscard]] bool HasEmergencyUrlSet() const
        {
            return remote.instance.has_value() ? remote.instance.value().emergencyUrl.has_value() : false;
        }

        /**
         * \brief Executes the emergency URL with the default local URL handler.
         */
        void LaunchEmergencySite() const;

        /**
         * \brief Returns the exit code check settings, if any.
         * \return The optional ExitCodeCheck.
         */
        std::optional<ExitCodeCheck> ExitCodeCheck()
        {
            // check if our release has the settings first
            if (const auto& release = GetSelectedRelease(); release.exitCode.has_value())
            {
                return release.exitCode;
            }

            // fallback to instance-global value, if any
            return remote.instance.has_value() ? remote.instance.value().exitCode : std::nullopt;
        }

        /**
         * \brief Attempts to display the UAC info dialog.
         * \param force True to ignore silent flags, false otherwise (default).
         */
        void TryDisplayUACDialog(bool force = false) const;

        /**
         * \brief Gets the help URL, if provided.
         * \return The optional help URL, if set.
         */
        std::optional<std::string> GetHelpUrl()
        {
            return remote.instance.has_value() ? remote.instance.value().helpUrl : std::nullopt;
        }

        /**
         * \brief Gets the help URL for installation failures, if provided.
         * \return The optional help URL, if set.
         */
        std::optional<std::string> GetInstallErrorUrl()
        {
            return merged.installationErrorUrl.empty() ? std::nullopt : std::optional(merged.installationErrorUrl);
        }

        /**
         * \brief Stores the current timestamp in a volatile registry key.
         */
        void SetPostponeData();

        /**
         * \brief Removes the postpone data from the registry, if any.
         */
        [[nodiscard]] std::expected<void, std::string> PurgePostponeData();

        /**
         * \brief Checks whether we're still in the postpone window.
         * \return True if we're still within a 24 hour window, false otherwise.
         */
        bool IsInPostponePeriod();

        /**
         * \brief Uses the template engine to render a string consisting of inja syntax.
         * \param tpl The template string.
         * \param data The data to process in the template.
         * \return The rendered string.
         */
        std::string RenderInjaTemplate(const std::string& tpl, const json& data) const;

        bool InvokeSetupAsync(const std::stop_token&);

        void ResetSetupState();

        /** Returned by GetSetupStatus when a setup task has been invoked. */
        struct SetupStatus
        {
            bool isRunning{};
            bool hasFinished{};
            std::optional<std::expected<SetupResult, std::string>> result{};
        };

        /** \brief Polls the in-progress setup task.
         *  \return nullopt if no task was started; otherwise a SetupStatus snapshot. */
        [[nodiscard]] std::optional<SetupStatus> GetSetupStatus() const;

        bool TryRunTemporaryProcess() const;

        int GetSuccessExitCode(int exitCode) const
        {
            return this->overrideSuccessCode ? this->overriddenSuccessCode : exitCode;
        }

        /**
         * \brief Verifies the downloaded setup file integrity (checksum + Authenticode publisher pin).
         * \return Empty on success; unexpected error string on failure.
         * \remarks Called between DownloadSucceeded and PrepareInstall in the UI state machine.
         *          Checksum: if present in the release it MUST match; absent = allowed in Relaxed, rejected in strict mode.
         *          Signature: governed by merged.signatureVerificationMode (WhenPresent / Required).
         */
        [[nodiscard]] std::expected<void, std::string> VerifyReleaseIntegrity();

        /**
         * \brief Validates the Authenticode signature of a file and optionally pins the publisher.
         * \param filePath The file to verify.
         * \param releaseSig Optional per-release certificate pin (overrides global strategy).
         * \param policy The comparison policy to apply.
         * \param strategy The pin strategy to use.
         * \return Empty on success; unexpected error string on failure.
         */
        [[nodiscard]] std::expected<void, std::string> VerifySetupSignature(
            const std::filesystem::path& filePath,
            const std::optional<models::SignatureConfig>& releaseSig,
            models::SignatureComparisonPolicy policy,
            models::SignatureVerificationStrategy strategy) const;

#if defined(NV_MANIFEST_PUBLIC_KEY)
        /**
         * \brief Verifies a detached minisign Ed25519 signature over the raw manifest body.
         * \param manifestBody The raw bytes of the fetched JSON.
         * \param minisigBody The raw contents of the .minisig sidecar file.
         * \return Empty on success; unexpected error string on failure.
         */
        [[nodiscard]] static std::expected<void, std::string> VerifyManifestSignature(
            const std::string& manifestBody,
            const std::string& minisigBody);

        /**
         * \brief Reads and enforces the manifest version anti-rollback counter from the registry.
         * \param signedVersion The monotonic version from the verified manifest.
         * \return True if the version is acceptable (>= stored), false if it is a rollback attempt.
         */
        [[nodiscard]] bool CheckAndUpdateManifestVersion(uint64_t signedVersion) const;
#endif
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstanceConfig, serverUrlTemplate, fallbackServerUrlTemplates, filenameRegex,
                                                    authority)
}
