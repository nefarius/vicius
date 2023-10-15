#pragma once
#include <string>
#include <fstream>
#include <regex>
#include <future>
#include <optional>

#include <nlohmann/json.hpp>
#include <restclient-cpp/connection.h>
#include <curl/curl.h>
#include <magic_enum.hpp>

#include "UpdateResponse.hpp"

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
		SharedConfig shared;
		/** The remote API response */
		UpdateResponse remote;

		std::optional<std::shared_future<int>> downloadTask;
		int selectedRelease{0};

		int DownloadRelease(curl_progress_callback progressFn, int releaseIndex);

		void SetCommonHeaders(RestClient::Connection* conn) const;

	public:
		std::string serverUrlTemplate;
		std::string filenameRegex;
		Authority authority;

		std::filesystem::path GetAppPath() const { return appPath; }
		semver::version GetAppVersion() const { return appVersion; }
		std::string GetAppFilename() const { return appFilename; }

		std::string GetTaskBarTitle() const { return shared.taskBarTitle; }
		std::string GetProductName() const { return shared.productName; }

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
		[[nodiscard]] bool RequestUpdateInfo();

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
			const auto latest = remote.instance.GetSemVersion();

			return latest > appVersion;
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
		bool IsInstalledVersionOutdated(bool& isOutdated);

		std::tuple<HRESULT, const char*> CreateScheduledTask(const std::string& launchArgs = "--background") const;

		std::tuple<HRESULT, const char*> RemoveScheduledTask() const;

		/**
		 * \brief Registers the updater executable in current users' autostart.
		 * \param launchArgs Optional launch arguments when run at autostart.
		 * \return True on success, false on error.
		 */
		bool RegisterAutostart(const std::string& launchArgs = "--autostart") const;

		/**
		 * \brief Removes the autostart entry for the current user.
		 * \return True on success, false otherwise.
		 */
		bool RemoveAutostart() const;

		InstanceConfig() : remote(), authority(Authority::Remote)
		{
		}

		InstanceConfig(const InstanceConfig&) = delete;
		InstanceConfig(InstanceConfig&&) = delete;
		InstanceConfig& operator=(const InstanceConfig&) = delete;
		InstanceConfig& operator=(InstanceConfig&&) = delete;

		InstanceConfig(HINSTANCE hInstance, argh::parser& cmdl);

		~InstanceConfig();
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstanceConfig, serverUrlTemplate, filenameRegex, authority)
}
