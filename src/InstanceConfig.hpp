#pragma once
#include <string>
#include <fstream>
#include <regex>

#include <nlohmann/json.hpp>
#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

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

	public:
		std::string serverUrlTemplate;
		std::string filenameRegex;
		Authority authority;

		std::filesystem::path GetAppPath() const { return appPath; }
		semver::version GetAppVersion() const { return appVersion; }
		std::string GetAppFilename() const { return appFilename; }

		std::string GetTaskBarTitle() const { return shared.taskBarTitle; }
		std::string GetProductName() const { return shared.productName; }

		UpdateRelease& GetLatestRelease()
		{
			return remote.releases[0];
		}

		/**
		 * \brief Requests the update configuration from the remote server.
		 * \return True on success, false otherwise.
		 */
		[[nodiscard]] bool RequestUpdateInfo()
		{
			const auto conn = new RestClient::Connection("");

			conn->SetTimeout(5);
			conn->SetUserAgent(std::format("{}/{}", appFilename, appVersion.to_string()));
			conn->FollowRedirects(true);
			conn->FollowRedirects(true, 5);

			RestClient::HeaderFields headers;
			headers["Accept"] = "application/json";
			conn->SetHeaders(headers);

			auto [code, body, _] = conn->get(updateRequestUrl);

			if (code != 200)
			{
				// TODO: add retry logic or similar
				return false;
			}

			try
			{
				const json reply = json::parse(body);
				remote = reply.get<UpdateResponse>();

				// top release is always latest by version, even if the response wasn't the right order
				std::ranges::sort(remote.releases, [](const auto& lhs, const auto& rhs)
				{
					return lhs.GetSemVersion() > rhs.GetSemVersion();
				});

				// bail out now if we are not supposed to obey the server settings
				if (authority == Local || !reply.contains("shared"))
				{
					return true;
				}

				// TODO: implement merging remote shared config

				shared = remote.shared;

				return true;
			}
			catch (...)
			{
				return false;
			}
		}

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

		InstanceConfig() : authority(Remote)
		{
		}

		InstanceConfig(const InstanceConfig&) = delete;
		InstanceConfig(InstanceConfig&&) = delete;
		InstanceConfig& operator=(const InstanceConfig&) = delete;
		InstanceConfig& operator=(InstanceConfig&&) = delete;

		explicit InstanceConfig(HINSTANCE hInstance) : appInstance(hInstance)
		{
			RestClient::init();

			//
			// Defaults and embedded stuff
			// 

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

			appPath = util::GetImageBasePathW();
			appVersion = util::GetVersionFromFile(appPath);
			appFilename = appPath.stem().string();
			filenameRegex = NV_FILENAME_REGEX;
			authority = Remote;

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
				}

				configFileStream.close();
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
				}
			}

			// first try to build "manufacturer/product" and use filename as 
			// fallback if extraction via regex didn't yield any results
			tenantSubPath = (!manufacturer.empty() && !product.empty())
				                ? std::format("{}/{}", manufacturer, product)
				                : appFilename;

			updateRequestUrl = std::vformat(serverUrlTemplate, std::make_format_args(tenantSubPath));
		}

		~InstanceConfig()
		{
			RestClient::disable();
		}
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstanceConfig, serverUrlTemplate, filenameRegex, authority)
}
