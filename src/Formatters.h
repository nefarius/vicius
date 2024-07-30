#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <filesystem>

template <> struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string>
{
    // Parse the format string (this part is usually left unchanged)
    template <typename ParseContext> constexpr auto parse(ParseContext& ctx) { return formatter<std::string>::parse(ctx); }

    // Format the argument
    template <typename FormatContext> auto format(const std::filesystem::path& p, FormatContext& ctx)
    {
        // Convert the path to a string and format it
        return formatter<std::string>::format(p.string(), ctx);
    }
};
