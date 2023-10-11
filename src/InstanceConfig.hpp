#pragma once
#include <string>
#include <fstream>
#include <regex>
#include <future>
#include <optional>

#include <nlohmann/json.hpp>
#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>
#include <curl/curl.h>

#include "UpdateResponse.hpp"

using json = nlohmann::json;

namespace models
{
	/**
	 * \brief If hitting duplicate instructions, which configuration gets the priority.
	 */
	enum Authority
	{
		///< The local config file (if any) gets priority
		Local,
		///< The server-side instructions get priority
		Remote,
		Invalid = -1
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(Authority, {
	                             {Invalid, nullptr},
	                             {Local, "Local"},
	                             {Remote, "Remote"},
	                             })

	/**
	 * \brief Local configuration file model.
	 */
	class InstanceConfig
	{
		HINSTANCE appInstance{};
		std::filesystem::path appPath;
		semver::version appVersion;
		std::string appFilename;
		std::string manufacturer;
		std::string product;
		std::string tenantSubPath;
		std::string updateRequestUrl;

		SharedConfig shared;
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

		bool SetSelectedRelease(const int releaseIndex)
		{
			if (releaseIndex < 0 || releaseIndex >= remote.releases.size())
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

		UpdateRelease& GetLatestRelease()
		{
			return remote.releases[0];
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
		[[nodiscard]] bool IsProductUpdateAvailable(const semver::version& currentVersion) const
		{
			if (remote.releases.empty())
			{
				return false;
			}

			const auto latest = remote.releases[0].GetSemVersion();

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
		bool DownloadReleaseAsync(int releaseIndex, curl_progress_callback progressFn)
		{
			// fail if already in-progress
			if (downloadTask.has_value())
			{
				return false;
			}

			downloadTask = std::async(
				std::launch::async,
				&InstanceConfig::DownloadRelease,
				this,
				progressFn,
				releaseIndex
			);

			return true;
		}

		/**
		 * \brief Checks the current download status.
		 * \param isDownloading True if a download is currently running in the background.
		 * \param hasFinished True if the download has finished (either with error or successful).
		 * \param statusCode The HTTP status code (set when hasFinished is true).
		 * \return True if a download has been invoked, false otherwise.
		 */
		[[nodiscard]] bool GetReleaseDownloadStatus(bool& isDownloading, bool& hasFinished, int& statusCode) const
		{
			if (!downloadTask.has_value())
			{
				return false;
			}

			const std::future_status status = (*downloadTask).wait_for(std::chrono::milliseconds(1));

			isDownloading = status == std::future_status::timeout;
			hasFinished = status == std::future_status::ready;

			if (hasFinished)
			{
				statusCode = (*downloadTask).get();				
			}

			return true;
		}

		void ResetReleaseDownloadState()
		{
			downloadTask.reset();
		}

		InstanceConfig() : remote(), authority(Remote)
		{
		}

		InstanceConfig(const InstanceConfig&) = delete;
		InstanceConfig(InstanceConfig&&) = delete;
		InstanceConfig& operator=(const InstanceConfig&) = delete;
		InstanceConfig& operator=(InstanceConfig&&) = delete;

		InstanceConfig(HINSTANCE hInstance);

		~InstanceConfig();
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstanceConfig, serverUrlTemplate, filenameRegex, authority)
}
