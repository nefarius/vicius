#include "Updater.h"
#include "InstanceConfig.hpp"

#include <fstream>
#include <cctype>
#include <algorithm>

#include <winreg/WinReg.hpp>
#include <neargye/semver.hpp>
#include <hash-library/md5.h>
#include <hash-library/sha1.h>
#include <hash-library/sha256.h>

#include <magic_enum.hpp>


models::InstanceConfig::InstanceConfig(HINSTANCE hInstance) : appInstance(hInstance), remote()
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

bool models::InstanceConfig::IsInstalledVersionOutdated(bool& isOutdated)
{
	const auto& release = GetSelectedRelease();

	switch (shared.detectionMethod)
	{
	//
	// Detect product version via registry key and value
	// 
	case ProductVersionDetectionMethod::RegistryValue:
		{
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
				return false;
			}

			const auto subKey = ConvertAnsiToWide(cfg.key);
			const auto valueName = ConvertAnsiToWide(cfg.value);

			winreg::RegKey key;

			if (const winreg::RegResult result = key.TryOpen(hive, subKey, KEY_READ); !result)
			{
				spdlog::error("Failed to open {}\\{} key", magic_enum::enum_name(cfg.hive), cfg.key);
				return false;
			}

			const auto& resource = key.TryGetStringValue(valueName);

			if (!resource.IsValid())
			{
				spdlog::error("Failed to access value {}", cfg.value);
				return false;
			}

			const std::wstring value = resource.GetValue();

			try
			{
				const semver::version localVersion{ConvertWideToANSI(value)};

				isOutdated = release.GetSemVersion() > localVersion;
			}
			catch (...)
			{
				spdlog::error("Failed to convert value {} into SemVer", ConvertWideToANSI(value));
				return false;
			}

			return true;
		}
	//
	// Detect product by comparing version resource
	// 
	case ProductVersionDetectionMethod::FileVersion:
		{
			const auto& cfg = shared.GetFileVersionConfig();

			try
			{
				isOutdated = release.GetSemVersion() > util::GetVersionFromFile(cfg.path);
			}
			catch (...)
			{
				spdlog::error("Failed to get version resource from {}", cfg.path);
				return false;
			}

			return true;
		}
	//
	// Detect product by comparing expected file sizes
	// 
	case ProductVersionDetectionMethod::FileSize:
		{
			const auto& cfg = shared.GetFileSizeConfig();

			try
			{
				const std::filesystem::path file{cfg.path};

				isOutdated = file_size(file) != cfg.size;
			}
			catch (...)
			{
				spdlog::error("Failed to get file size from {}", cfg.path);
				return false;
			}

			return true;
		}
	//
	// Detect product by hashing a given file checksum
	// 
	case ProductVersionDetectionMethod::FileChecksum:
		{
			const auto& cfg = shared.GetFileChecksumConfig();

			if (!std::filesystem::exists(cfg.path))
			{
				spdlog::error("File {} doesn't exist", cfg.path);
				return false;
			}

			std::ifstream file(cfg.path, std::ios::binary);

			if (!file.is_open())
			{
				spdlog::error("Failed to open file {}", cfg.path);
				return false;
			}

			constexpr std::size_t chunkSize = 4 * 1024; // 4 KB

			switch (cfg.algorithm)
			{
			case ChecksumAlgorithm::MD5:
				{
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
								return false;
							}
						}
					}

					isOutdated = !util::icompare(alg.getHash(), cfg.hash);

					return true;
				}
			case ChecksumAlgorithm::SHA1:
				{
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
								return false;
							}
						}
					}

					isOutdated = !util::icompare(alg.getHash(), cfg.hash);

					return true;
				}
			case ChecksumAlgorithm::SHA256:
				{
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
								return false;
							}
						}
					}

					isOutdated = !util::icompare(alg.getHash(), cfg.hash);

					return true;
				}
			case ChecksumAlgorithm::Invalid:
				spdlog::error("Invalid hashing algorithm specified");
				return false;
			}

			file.close();

			break;
		}
	case ProductVersionDetectionMethod::Invalid:
		spdlog::error("Invalid detection method specified");
		return false;
	}

	spdlog::error("No detection method matched");
	return false;
}

bool models::InstanceConfig::RegisterAutostart(const std::string& launchArgs) const
{
	winreg::RegKey key;
	const auto subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

	if (const winreg::RegResult result = key.TryOpen(HKEY_CURRENT_USER, subKey); !result)
	{
		spdlog::error("Failed to open {}", ConvertWideToANSI(subKey));
		return false;
	}

	std::stringstream ss;
	ss << "\"" << appPath.string() << "\" " << launchArgs;

	if (const auto writeResult = key.TrySetStringValue(
			ConvertAnsiToWide(appFilename),
			ConvertAnsiToWide(ss.str()));
		!writeResult)
	{
		spdlog::error("Failed to write autostart value");
		return false;
	}

	return true;
}
