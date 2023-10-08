#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace models
{
	class InstanceConfig
	{
	private:
		std::string serverUrlTemplate;
		std::string filenameRegex;

	public:
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(InstanceConfig, serverUrlTemplate, filenameRegex)

		InstanceConfig()
		{
		}

		InstanceConfig(std::string url) : serverUrlTemplate(std::move(url))
		{
			filenameRegex = NV_FILENAME_REGEX;
		}

		const std::string& GetFilenameRegex() const { return filenameRegex; }
	};
}
