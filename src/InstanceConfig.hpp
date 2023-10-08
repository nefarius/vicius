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

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstanceConfig, serverUrlTemplate, filenameRegex)
}
