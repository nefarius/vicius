#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <neargye/semver.hpp>

namespace models
{
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
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateConfig, updatesDisabled)

	/**
	 * \brief An instance returned by the remote update API.
	 */
	class UpdateResponse
	{
	public:
		/** The global settings instance */
		UpdateConfig instance;
		/** The available releases */
		std::vector<UpdateRelease> releases;

		[[nodiscard]] bool IsUpdateAvailable(const semver::version& currentVersion) const
		{
			if (releases.empty())
			{
				return false;
			}

			const auto latest = releases[0].GetSemVersion();

			return latest > currentVersion;
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UpdateResponse, instance, releases)
}
