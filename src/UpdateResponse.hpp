#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <neargye/semver.hpp>
#include <magic_enum.hpp>

#include "Updater.h"

using json = nlohmann::json;

namespace models
{
	/**
	 * \brief Possible hashing algorithms for file checksums.
	 */
	enum class ChecksumAlgorithm
	{
		MD5,
		SHA1,
		SHA256,
		SHA512,
		Invalid = -1
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(ChecksumAlgorithm, {
	                             {ChecksumAlgorithm::Invalid, nullptr},
	                             {ChecksumAlgorithm::MD5,
	                             magic_enum::enum_name(ChecksumAlgorithm::MD5)},
	                             {ChecksumAlgorithm::SHA1,
	                             magic_enum::enum_name(ChecksumAlgorithm::SHA1)},
	                             {ChecksumAlgorithm::SHA256,
	                             magic_enum::enum_name(ChecksumAlgorithm::SHA256)},
	                             {ChecksumAlgorithm::SHA512,
	                             magic_enum::enum_name(ChecksumAlgorithm::SHA512)},
	                             })

	/**
	 * \brief Possible installed product detection mechanisms.
	 */
	enum class ProductVersionDetectionMethod
	{
		RegistryValue,
		FileVersion,
		FileSize,
		Invalid = -1
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(ProductVersionDetectionMethod, {
	                             {ProductVersionDetectionMethod::Invalid, nullptr},
	                             {ProductVersionDetectionMethod::RegistryValue,
	                             magic_enum::enum_name(ProductVersionDetectionMethod::RegistryValue)},
	                             {ProductVersionDetectionMethod::FileVersion,
	                             magic_enum::enum_name(ProductVersionDetectionMethod::FileVersion)},
	                             {ProductVersionDetectionMethod::FileSize,
	                             magic_enum::enum_name(ProductVersionDetectionMethod::FileSize)},
	                             })

	/**
	 * \brief Possible registry hives.
	 */
	enum class RegistryHive
	{
		HKCU = reinterpret_cast<ULONG_PTR>(HKEY_CURRENT_USER),
		HKLM = reinterpret_cast<ULONG_PTR>(HKEY_LOCAL_MACHINE),
		HKCR = reinterpret_cast<ULONG_PTR>(HKEY_CLASSES_ROOT),
		Invalid = -1
	};

	NLOHMANN_JSON_SERIALIZE_ENUM(RegistryHive, {
	                             {RegistryHive::Invalid, nullptr},
	                             {RegistryHive::HKCU, magic_enum::enum_name(RegistryHive::HKCU)},
	                             {RegistryHive::HKLM, magic_enum::enum_name(RegistryHive::HKLM)},
	                             {RegistryHive::HKCR, magic_enum::enum_name(RegistryHive::HKCR)},
	                             })

	class RegistryValueConfig
	{
	public:
		RegistryHive hive;
		std::string key;
		std::string value;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RegistryValueConfig, hive, key, value)

	class FileVersionConfig
	{
	public:
		std::string path;
		std::string version;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FileVersionConfig, path, version)

	class FileSizeConfig
	{
	public:
		std::string path;
		size_t size;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FileSizeConfig, path, size)

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
		/** The publishing timestamp as UTC ISO 8601 string */
		std::string publishedAt;
		/** URL of the new setup/release download */
		std::string downloadUrl;
		/** Size of the remote file */
		size_t downloadSize;
		/** The launch arguments (CLI arguments) if any */
		std::string launchArguments{};
		/** The setup exit codes treated as success */
		std::vector<int> successExitCodes;
		/** True to skip exit code check */
		bool skipExitCodeCheck;
		/** The (optional) checksum of the remote setup */
		std::string checksum;
		/** The checksum algorithm */
		ChecksumAlgorithm checksumAlg;
		/** The detection method */
		ProductVersionDetectionMethod detectionMethod;
		/** The detection method for the installed software version */
		json detection;

		/** Full pathname of the local temporary file */
		std::filesystem::path localTempFilePath{};

		/**
		 * \brief Converts the version string to a SemVer type.
		 * \return The parsed version.
		 */
		semver::version GetSemVersion() const
		{
			try
			{
				// trim whitespaces and potential "v" prefix
				return semver::version{util::trim(version, "v \t")};
			}
			catch (...)
			{
				return semver::version{0, 0, 0};
			}
		}

		RegistryValueConfig GetRegistryValueConfig() const
		{
			return detection.get<RegistryValueConfig>();
		}

		FileVersionConfig GetFileVersionConfig() const
		{
			return detection.get<FileVersionConfig>();
		}

		FileSizeConfig GetFileSizeConfig() const
		{
			return detection.get<FileSizeConfig>();
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		UpdateRelease,
		name,
		version,
		summary,
		publishedAt,
		downloadUrl,
		downloadSize,
		launchArguments,
		successExitCodes,
		skipExitCodeCheck,
		checksum,
		checksumAlg,
		detectionMethod,
		detection
	)

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

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		UpdateConfig,
		updatesDisabled,
		latestVersion,
		latestUrl
	)

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

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		UpdateResponse,
		instance,
		shared,
		releases
	)
}
