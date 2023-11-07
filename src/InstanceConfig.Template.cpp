#include "pch.h"
#include "InstanceConfig.hpp"
#include <inja/inja.hpp>


std::string models::InstanceConfig::RenderInjaTemplate(const std::string& tpl, const json& data) const
{
    inja::Environment env;

    // provides reading environment variable, example: envar("PROGRAMDATA") -> C:\ProgramData
    env.add_callback("envar", [](const inja::Arguments& args)
    {
        if (args.empty())
        {
            return std::string{};
        }

        const auto envarName = args.at(0)->get<std::string>();

        std::string envarValue(MAX_PATH, '\0');

        if (!GetEnvironmentVariableA(envarName.c_str(), envarValue.data(), static_cast<DWORD>(envarValue.size())))
        {
            // return fallback/default value on failure, if provided
            return args.size() > 1 ? args.at(1)->get<std::string>() : std::string{};
        }

        // strip redundant NULLs
        envarValue.erase(std::ranges::find(envarValue, '\0'), envarValue.end());

        return envarValue;
    });

    try
    {
        auto rendered = env.render(tpl, data);

        spdlog::debug("rendered = {}", rendered);

        return rendered;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Failed to render inja template, error {}", ex.what());

        return std::string{};
    }
}
