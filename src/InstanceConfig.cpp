#include "Updater.h"
#include "InstanceConfig.hpp"


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

models::InstanceConfig::~InstanceConfig()
{
	RestClient::disable();
}
