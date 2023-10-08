#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace models
{
	struct InstanceConfig
	{
		std::string serverUrlTemplate;
		std::string filenameRegex;		
	};
}
