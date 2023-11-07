#include "pch.h"
#include "InstanceConfig.hpp"
#include <inja/inja.hpp>


std::string models::InstanceConfig::RenderInjaTemplate(const std::string& tpl, const json& data) const
{
    inja::Environment env;

    // provides reading environment variable, example: envar("PROGRAMDATA") -> C:\ProgramData
    env.add_callback("envar", 1, [](const inja::Arguments& args)
    {
        const auto envarName = args.at(0)->get<std::string>();

        std::string envarValue(MAX_PATH, '\0');

        GetEnvironmentVariableA(envarName.c_str(), envarValue.data(), static_cast<DWORD>(envarValue.size()));

        // strip redundant NULLs
        envarValue.erase(std::ranges::find(envarValue, '\0'), envarValue.end());

        return envarValue;
    });

    auto rendered = env.render(tpl, data);

    spdlog::debug("rendered = {}", rendered);

    return rendered;
}
