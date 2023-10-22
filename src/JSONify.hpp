#pragma once

#include <nlohmann/json.hpp>
 
#include <optional>
#include <variant>

/*
 * Source: https://www.kdab.com/jsonify-with-nlohmann-json/
 * */

namespace nlohmann {
 
///////////////////////////////////////////////////////////////////////////////
// std::variant
///////////////////////////////////////////////////////////////////////////////
// Try to set the value of type T into the variant data if it fails, do nothing
template <typename T, typename... Ts>
void variant_from_json(const nlohmann::json &j, std::variant<Ts...> &data) {
    try {
        data = j.get<T>();
    } catch (...) {
    }
}
 
template <typename... Ts>
struct adl_serializer<std::variant<Ts...>>
{
    static void to_json(nlohmann::json &j, const std::variant<Ts...> &data) {
        // Will call j = v automatically for the right type
        std::visit([&j](const auto &v) { j = v; }, data);
    }
 
    static void from_json(const nlohmann::json &j, std::variant<Ts...> &data) {
        // Call variant_from_json for all types, only one will succeed
        (variant_from_json<Ts>(j, data), ...);
    }
};
///////////////////////////////////////////////////////////////////////////////
// std::optional
///////////////////////////////////////////////////////////////////////////////
template <class T>
void optional_to_json(nlohmann::json &j, const char *name, const std::optional<T> &value) {
    if (value)
        j[name] = *value;
}
template <class T>
void optional_from_json(const nlohmann::json &j, const char *name, std::optional<T> &value) {
    const auto it = j.find(name);
    if (it != j.end())
        value = it->get<T>();
    else
        value = std::nullopt;
}
 
///////////////////////////////////////////////////////////////////////////////
// all together
///////////////////////////////////////////////////////////////////////////////
template <typename>
constexpr bool is_optional = false;
template <typename T>
constexpr bool is_optional<std::optional<T>> = true;
 
template <typename T>
void extended_to_json(const char *key, nlohmann::json &j, const T &value) {
    if constexpr (is_optional<T>)
        nlohmann::optional_to_json(j, key, value);
    else
        j[key] = value;
}
template <typename T>
void extended_from_json(const char *key, const nlohmann::json &j, T &value) {
    if constexpr (is_optional<T>)
        nlohmann::optional_from_json(j, key, value);
    else
        j.at(key).get_to(value);
}
 
}
 
#define EXTEND_JSON_TO(v1) extended_to_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
#define EXTEND_JSON_FROM(v1) extended_from_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
 
#define NLOHMANN_JSONIFY_ALL_THINGS(Type, ...)                                          \
  inline void to_json(nlohmann::json &nlohmann_json_j, const Type &nlohmann_json_t) {   \ 
      NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(EXTEND_JSON_TO, __VA_ARGS__))            \
  }                                                                                     \
  inline void from_json(const nlohmann::json &nlohmann_json_j, Type &nlohmann_json_t) { \
      NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(EXTEND_JSON_FROM, __VA_ARGS__))          \
  }
