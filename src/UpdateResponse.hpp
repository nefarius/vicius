#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <neargye/semver.hpp>

namespace models
{
	/**
	 * \brief Parameters that might be provided by both the server and the local configuration.
	 */
	class SharedConfig
	{
	public:
		std::string taskBarTitle;
		std::string productName;

		SharedConfig() : taskBarTitle(NV_TASKBAR_TITLE), productName(NV_PRODUCT_NAME)
		{
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SharedConfig, taskBarTitle, productName)

	/**
	 * \brief Represents an update release.
	 */
	class UpdateRelease
	{
	public:
		/** The release display name */
		std::string name;
		/** The version as a 3-digit SemVer or 4-digit string */
		std::string version;
		/** The update summary/changelog, supports Markdown */
		std::string summary;
		/** The publishing timestamp as a UTC ISO-8601 string */
		std::string publishedAt;

		/**
		 * \brief Converts the version string to a SemVer type.
		 * \return The parsed version.
		 */
		semver::version GetSemVersion() const
		{
			try
			{
				return semver::version{version};
			}
			catch (...)
			{
				return semver::version{0, 0, 0};
			}
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateRelease, name, version, summary, publishedAt)

	/**
	 * \brief Update instance configuration. Parameters applying to the entire product/tenant.
	 */
	class UpdateConfig
	{
	public:
		/** True to disable, false to enable the updates globally */
		bool updatesDisabled;
		/** The latest updater version available */
		std::string latestVersion;
		/** URL of the latest updater binary */
		std::string latestUrl;

		/**
		 * \brief Converts the version string to a SemVer type.
		 * \return The parsed version.
		 */
		semver::version GetSemVersion() const
		{
			try
			{
				return semver::version{latestVersion};
			}
			catch (...)
			{
				return semver::version{0, 0, 0};
			}
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateConfig, updatesDisabled, latestVersion, latestUrl)

	/**
	 * \brief An instance returned by the remote update API.
	 */
	class UpdateResponse
	{
	public:
		/** The global settings instance */
		UpdateConfig instance;
		/** The (optional) shared settings */
		SharedConfig shared;
		/** The available releases */
		std::vector<UpdateRelease> releases;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateResponse, instance, shared, releases)
}
