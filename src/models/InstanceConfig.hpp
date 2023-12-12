#pragma once
#include <restclient-cpp/connection.h>
#include <curl/curl.h>

#include "UpdateResponse.hpp"
#include "MergedConfig.hpp"
#include "NAuthenticode.h"

using json = nlohmann::json;

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

    NLOHMANN_JSON_SERIALIZE_ENUM(Authority, {
                                 {Authority::Invalid, nullptr},
                                 {Authority::Local,
                                 magic_enum::enum_name(Authority::Local)},
                                 {Authority::Remote,
                                 magic_enum::enum_name(Authority::Remote)},
                                 })

    /**
     * \brief Local configuration file model.
     */
    class InstanceConfig
    {
        HINSTANCE appInstance{};
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

        /** The local and remote shared configuration */
        MergedConfig merged;
        /** The remote API response */
        UpdateResponse remote;

        std::optional<std::shared_future<int>> downloadTask;
        std::optional<std::shared_future<std::tuple<bool, DWORD, DWORD>>> setupTask;
        int selectedRelease{0};
        bool isSilent{false};
        NSIGINFO appSigInfo{};

        int DownloadRelease(curl_progress_callback progressFn, int releaseIndex);

        void SetCommonHeaders(RestClient::Connection* conn) const;

        std::tuple<bool, DWORD, DWORD> ExecuteSetup();

    public:
        std::string serverUrlTemplate;
        std::string filenameRegex;
        Authority authority;
        std::string channel;
        std::map<std::string, std::string> additionalHeaders;

        std::filesystem::path GetAppPath() const { return appPath; }
        semver::version GetAppVersion() const { return appVersion; }
        std::string GetAppFilename() const { return appFilename; }

        std::string GetWindowTitle() const { return merged.windowTitle; }
        std::string GetProductName() const { return merged.productName; }

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
            return remote.releases[releaseId].localTempFilePath;
        }

        UpdateRelease& GetSelectedRelease()
        {
            return remote.releases[selectedRelease];
        }

        int GetSelectedReleaseId() const
        {
            return selectedRelease;
        }

        /**
         * \brief Requests the update configuration from the remote server.
         * \return True on success, false otherwise.
         */
        [[nodiscard]] std::tuple<bool, std::string> RequestUpdateInfo();

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

            if (remote.instance.value().latestVersion.has_value() &&
                remote.instance.value().latestUrl.has_value())
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
        [[nodiscard]] bool HasSingleRelease() const
        {
            return remote.releases.size() == 1;
        }

        /**
         * \brief Checks if we have multiple update releases.
         * \return True if multiple update releases, false otherwise.
         */
        [[nodiscard]] bool HasMultipleReleases() const
        {
            return remote.releases.size() > 1;
        }

        /**
         * \brief Starts the update release download.
         * \param releaseIndex Zero-based release index.
         * \param progressFn The download progress callback.
         */
        bool DownloadReleaseAsync(int releaseIndex, curl_progress_callback progressFn);

        /**
         * \brief Checks the current download status.
         * \param isDownloading True if a download is currently running in the background.
         * \param hasFinished True if the download has finished (either with error or successful).
         * \param statusCode The HTTP status code (set when hasFinished is true).
         * \return True if a download has been invoked, false otherwise.
         */
        [[nodiscard]] bool GetReleaseDownloadStatus(bool& isDownloading, bool& hasFinished, int& statusCode) const;

        /**
         * \brief Reset the download async task state.
         */
        void ResetReleaseDownloadState();

        /**
         * \brief Checks the version of the installed product against the latest available release.
         * \param isOutdated True if the detected installed version is older than the latest server release.
         * \return True if detection succeeded, false on error.
         */
        std::tuple<bool, std::string> IsInstalledVersionOutdated(bool& isOutdated);

        std::tuple<HRESULT, std::string> CreateScheduledTask(const std::string& launchArgs = NV_CLI_BACKGROUND) const;

        std::tuple<HRESULT, std::string> RemoveScheduledTask() const;

        /**
         * \brief Registers the updater executable in current users' autostart.
         * \param launchArgs Optional launch arguments when run at autostart.
         * \return True on success, false on error.
         */
        std::tuple<bool, std::string> RegisterAutostart(const std::string& launchArgs = NV_CLI_AUTOSTART) const;

        /**
         * \brief Removes the autostart entry for the current user.
         * \return True on success, false otherwise.
         */
        std::tuple<bool, std::string> RemoveAutostart() const;

        /**
         * \brief Extracts the embedded self-updater DLL resource into our own app as ADS.
         * \return True on success, false otherwise.
         */
        std::tuple<bool, std::string> ExtractSelfUpdater() const;

        /**
         * \brief Checks if the current app working directory can be written to.
         * \return True if we can write, false otherwise.
         */
        bool HasWritePermissions() const;

        /**
         * \brief Attempts to spawn the self-updater component.
         * \return True on success (end this process if the case), false on error.
         */
        bool RunSelfUpdater() const;

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

        bool InvokeSetupAsync();

        void ResetSetupState();

        [[nodiscard]] bool GetSetupStatus(
            bool& isRunning,
            bool& hasFinished,
            bool& hasSucceeded,
            DWORD& exitCode,
            DWORD& win32Error
        ) const;

        InstanceConfig() : authority(Authority::Remote)
        {
        }

        InstanceConfig(const InstanceConfig&) = delete;
        InstanceConfig(InstanceConfig&&) = delete;
        InstanceConfig& operator=(const InstanceConfig&) = delete;
        InstanceConfig& operator=(InstanceConfig&&) = delete;

        InstanceConfig(HINSTANCE hInstance, argh::parser& cmdl);

        ~InstanceConfig();
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        InstanceConfig,
        serverUrlTemplate,
        filenameRegex,
        authority
    )
}
