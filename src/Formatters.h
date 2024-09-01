#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <filesystem>

namespace fmt
{
    // Custom formatter for std::filesystem::path
    template <>
    struct formatter<std::filesystem::path> : formatter<std::string>
    {
        // Parse function is inherited from formatter<std::string>
        template <typename FormatContext>
        auto format(const std::filesystem::path& path, FormatContext& ctx) const
        {
            return formatter<std::string>::format(path.string(), ctx);
        }
    };

} // namespace fmt
