#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

NLOHMANN_JSON_NAMESPACE_BEGIN
    /**
     * \brief https://github.com/nlohmann/json/issues/1749#issuecomment-1731266676
     */
    template <typename T>
    struct adl_serializer<std::optional<T>>
    {
        static void from_json(const json& j, std::optional<T>& opt)
        {
            if (j.is_null())
            {
                opt = std::nullopt;
            }
            else
            {
                opt = j.get<T>();
            }
        }

        static void to_json(json& json, std::optional<T> t)
        {
            if (t)
            {
                json = *t;
            }
            else
            {
                json = nullptr;
            }
        }
    };

NLOHMANN_JSON_NAMESPACE_END
