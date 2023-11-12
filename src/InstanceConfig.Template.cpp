#include "pch.h"
#include "InstanceConfig.hpp"
#pragma warning(disable: 4244)
#include <inja/inja.hpp>
#pragma warning(default: 4244)
#include "inipp.h"


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

    // reads a registry value, mandatory params:
    // - view
    // - hive
    // - key
    // - value
    //
    // optional param:
    // - default/fallback value if not found
    env.add_callback("regval", [](const inja::Arguments& args)
    {
        std::string defaultRet{};

        if (args.size() < 4)
        {
            return defaultRet;
        }

        if (args.size() > 4)
        {
            defaultRet = args.at(4)->get<std::string>();
        }

        const auto viewName = args.at(0)->get<std::string>();
        const auto view = magic_enum::enum_cast<RegistryView>(viewName);
        const auto hiveName = args.at(1)->get<std::string>();
        const auto subKeyValue = args.at(2)->get<std::string>();
        const auto subKey = ConvertAnsiToWide(subKeyValue);
        const auto valueNameValue = args.at(3)->get<std::string>();
        const auto valueName = ConvertAnsiToWide(valueNameValue);

        HKEY hive = nullptr;

        const auto regHive = magic_enum::enum_cast<RegistryHive>(hiveName);

        if (!regHive.has_value())
        {
            spdlog::error("Unsupported hive provided");
            return defaultRet;
        }

        switch (regHive.value())
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
            spdlog::error("Unsupported hive provided");
            return defaultRet;
        }

        winreg::RegKey key;
        REGSAM flags = KEY_READ;

        if (view.has_value() && view > RegistryView::Default)
        {
            flags |= static_cast<REGSAM>(view.value());
        }

        if (const winreg::RegResult result = key.TryOpen(hive, subKey, flags); !result)
        {
            spdlog::error("Failed to open {}\\{} key", hiveName, subKeyValue);
            return defaultRet;
        }

        const auto& type = key.TryQueryValueType(valueName);

        if (!type.IsValid())
        {
            spdlog::error("Couldn't get type of vale {}", valueNameValue);
            return defaultRet;
        }

        std::stringstream valStream;

        switch (type.GetValue())
        {
        case REG_BINARY:
            {
                const auto ret = key.TryGetBinaryValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                // convert to hex stream string
                json j = json::array();
                for (int i = 0; i < ret.GetValue().size(); ++i)
                {
                    j.emplace_back(ret.GetValue().data()[i]);
                }

                valStream << j;

                break;
            }
        case REG_DWORD:
            {
                const auto ret = key.TryGetDwordValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                valStream << ret.GetValue();

                break;
            }
        case REG_EXPAND_SZ:
            {
                const auto ret = key.TryGetExpandStringValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                std::wstring expandedValue(SHRT_MAX, '\0');

                if (ExpandEnvironmentStrings(ret.GetValue().c_str(), expandedValue.data(), SHRT_MAX))
                {
                    // strip redundant NULLs
                    expandedValue.erase(std::ranges::find(expandedValue, '\0'), expandedValue.end());
                    valStream << ConvertWideToANSI(expandedValue);
                }
                else
                {
                    spdlog::warn("Failed to expand string {}", ConvertWideToANSI(ret.GetValue()));
                    valStream << ConvertWideToANSI(ret.GetValue());
                }

                break;
            }
        case REG_MULTI_SZ:
            {
                const auto ret = key.TryGetMultiStringValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                json j = json::array();
                for (int i = 0; i < ret.GetValue().size(); ++i)
                {
                    j.emplace_back(ConvertWideToANSI(ret.GetValue()[i]));
                }

                valStream << j;

                break;
            }
        case REG_QWORD:
            {
                const auto ret = key.TryGetQwordValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                valStream << ret.GetValue();

                break;
            }
        case REG_SZ:
            {
                const auto ret = key.TryGetStringValue(valueName);

                if (!ret.IsValid())
                {
                    spdlog::error("Failed to access value {}", valueNameValue);
                    return defaultRet;
                }

                valStream << ConvertWideToANSI(ret.GetValue());

                break;
            }
        default:
            spdlog::error("Unsupported value type {} detected", type.GetValue());
            return defaultRet;
        }

        std::string value = valStream.str();

        spdlog::debug("value = {}", value);

        return value;
    });

    // reads a key value from a section of an INI file
    env.add_callback("inival", [](const inja::Arguments& args)
    {
        std::string defaultRet{};

        if (args.size() < 3)
        {
            return defaultRet;
        }

        if (args.size() > 3)
        {
            defaultRet = args.at(3)->get<std::string>();
        }

        const auto filePath = args.at(0)->get<std::string>();
        const auto section = args.at(1)->get<std::string>();
        const auto keyName = args.at(2)->get<std::string>();

        try
        {
            if (std::filesystem::file_size(filePath) <= 3)
            {
                spdlog::warn("File {} is too small for successful parsing", filePath);
                return defaultRet;
            }

            inipp::Ini<char> ini;
            std::ifstream is(filePath);

            char a, b, c;
            a = is.get();
            b = is.get();
            c = is.get();
            // checks for UTF-8 BOM presence and skips it (1st found section extraction will fail otherwise)
            if (a != static_cast<char>(0xEF) || b != static_cast<char>(0xBB) || c != static_cast<char>(0xBF))
            {
                is.seekg(0);
            }

            ini.parse(is);

            std::string value;
            if (!inipp::get_value(ini.sections[section], keyName, value))
            {
                spdlog::warn("Section {} not found", section);
                return defaultRet;
            }

            return value;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to read INI file {}, error {}", filePath, e.what());
            return defaultRet;
        }
    });

    // simple logging function for debugging
    env.add_void_callback("log", 1, [](const inja::Arguments& args)
    {
        const auto logMessage = args.at(0)->get<std::string>();

        spdlog::debug("inja log message: {}", logMessage);
    });

    // searches for installed products by regular expression
    env.add_callback("productBy", 2, [](const inja::Arguments& args)
    {
        const auto targetValue = args.at(0)->get<std::string>();
        const auto expression = args.at(1)->get<std::string>();

        json j = json::object();
        j["count"] = std::to_string(0);
        json results = json::array();

        winreg::RegKey key;
        REGSAM flags = KEY_READ;

        const auto baseKey = LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall)";

        if (const winreg::RegResult result = key.TryOpen(HKEY_LOCAL_MACHINE, baseKey, flags); !result)
        {
            spdlog::error("Failed to open uninstall key");
            j["error"] = "Failed to open uninstall key";
            return j;
        }

        const auto enumRet = key.TryEnumSubKeys();

        if (!enumRet.IsValid())
        {
            spdlog::error("Failed to enumerate sub-keys");
            j["error"] = "Failed to enumerate sub-keys";
            return j;
        }

        // enumerate each sub-key (each represents an installed product)
        for (const auto& subKeyName : enumRet.GetValue())
        {
            // build full new path
            const auto subKeyPath = std::format(R"({}\{})", ConvertWideToANSI(baseKey), ConvertWideToANSI(subKeyName));

            winreg::RegKey subKey;
            const winreg::RegResult subResult = subKey.
                TryOpen(HKEY_LOCAL_MACHINE, ConvertAnsiToWide(subKeyPath), flags);

            if (subResult.Failed())
            {
                continue;
            }

            // attempts to extract the value by name provided by the caller
            const auto& value = subKey.TryGetStringValue(ConvertAnsiToWide(targetValue));

            if (!value.IsValid())
            {
                continue;
            }

            // regex match to queried value
            std::string displayName = ConvertWideToANSI(value.GetValue());
            std::regex productRegex(expression, std::regex_constants::icase);
            auto matchesBegin = std::sregex_iterator(displayName.begin(), displayName.end(), productRegex);
            auto matchesEnd = std::sregex_iterator();

            if (matchesBegin == matchesEnd)
            {
                continue;
            }

            json entry = json::object();

            entry[targetValue] = displayName;

            //
            // Query for well-known properties
            // 

            if (const auto& ret = subKey.TryGetStringValue(L"InstallLocation"); ret)
            {
                entry["installLocation"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetStringValue(L"DisplayVersion"); ret)
            {
                entry["displayVersion"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetStringValue(L"Publisher"); ret)
            {
                entry["publisher"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetStringValue(L"InstallSource"); ret)
            {
                entry["installSource"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetStringValue(L"InstallDate"); ret)
            {
                entry["installDate"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetStringValue(L"UninstallString"); ret)
            {
                entry["uninstallString"] = ConvertWideToANSI(ret.GetValue());
            }

            if (const auto& ret = subKey.TryGetDwordValue(L"Language"); ret)
            {
                entry["language"] = std::to_string(ret.GetValue());
            }

            results.push_back(entry);
        }

        j["count"] = std::to_string(results.size());
        j["results"] = results;

        std::string value = j.dump(4, ' ', false, json::error_handler_t::replace);

        spdlog::debug("value = {}", value);

        return j;
    });


    try
    {
        auto rendered = env.render(tpl, data);

        spdlog::debug("rendered = {}", rendered);

        return rendered;
    }
    catch (const inja::ParserError& ex)
    {
        spdlog::error("Failed to render inja template, error {}", ex.what());

        return std::string{};
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Failed to render inja template, error {}", ex.what());

        return std::string{};
    }
}
