#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Dependency-free payload classification. The unit-test project compiles this header
// directly, so it must not include Windows headers or anything from vcpkg.

namespace setuppayload
{

    /**
     * \brief Extension of the last path segment of a download URL, without the dot.
     *
     * The fragment and query are discarded first. The extension is accepted only when it
     * is 1 to 8 ASCII letters or digits, and it is returned lowercased. Anything else
     * (no extension, separators, traversal, overlong or punctuated junk) is rejected so
     * it can never become part of a file name.
     */
    [[nodiscard]] inline std::optional<std::string> ExtensionFromUrl(std::string_view url)
    {
        const auto hash = url.find('#');
        if (hash != std::string_view::npos)
        {
            url = url.substr(0, hash);
        }

        const auto query = url.find('?');
        if (query != std::string_view::npos)
        {
            url = url.substr(0, query);
        }

        while (!url.empty() && (url.back() == '/' || url.back() == '\\'))
        {
            url.remove_suffix(1);
        }

        if (url.empty())
        {
            return std::nullopt;
        }

        const auto slash = url.find_last_of("/\\");
        const auto segment = slash == std::string_view::npos ? url : url.substr(slash + 1);
        if (segment.empty() || segment == "." || segment == "..")
        {
            return std::nullopt;
        }

        const auto dot = segment.rfind('.');
        if (dot == std::string_view::npos || dot + 1 >= segment.size())
        {
            return std::nullopt;
        }

        const auto extension = segment.substr(dot + 1);
        if (extension.size() > 8)
        {
            return std::nullopt;
        }

        std::string lower;
        lower.reserve(extension.size());
        for (const char ch : extension)
        {
            const auto c = static_cast<unsigned char>(ch);
            if (c >= 'A' && c <= 'Z')
            {
                lower.push_back(static_cast<char>(c - 'A' + 'a'));
            }
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                lower.push_back(static_cast<char>(c));
            }
            else
            {
                return std::nullopt;
            }
        }

        return lower;
    }

    /**
     * \brief True when the bytes start with the OLE compound-file signature every MSI uses.
     */
    [[nodiscard]] inline bool LooksLikeCompoundFile(std::span<const std::byte> bytes)
    {
        static constexpr unsigned char kSignature[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
        if (bytes.size() < sizeof(kSignature))
        {
            return false;
        }

        for (std::size_t i = 0; i < sizeof(kSignature); ++i)
        {
            if (std::to_integer<unsigned char>(bytes[i]) != kSignature[i])
            {
                return false;
            }
        }

        return true;
    }

    /**
     * \brief True when the bytes look like HTML, XML, JSON, or other plain text.
     *
     * This is the shape of a captive portal, an error page, or a JSON error body saved
     * where an installer was expected. A NUL byte (typical of PE and other binaries)
     * is treated as not text. A UTF-16 BOM is treated as text.
     */
    [[nodiscard]] inline bool LooksLikeText(std::span<const std::byte> bytes)
    {
        if (bytes.empty())
        {
            return false;
        }

        if (bytes.size() >= 2)
        {
            const auto b0 = std::to_integer<unsigned char>(bytes[0]);
            const auto b1 = std::to_integer<unsigned char>(bytes[1]);
            if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF))
            {
                return true;
            }
        }

        std::size_t index = 0;
        if (bytes.size() >= 3 && std::to_integer<unsigned char>(bytes[0]) == 0xEF &&
            std::to_integer<unsigned char>(bytes[1]) == 0xBB && std::to_integer<unsigned char>(bytes[2]) == 0xBF)
        {
            index = 3;
        }

        for (std::size_t i = index; i < bytes.size(); ++i)
        {
            if (bytes[i] == std::byte{0})
            {
                return false;
            }
        }

        const auto isSpace = [](std::byte b) {
            const auto c = std::to_integer<unsigned char>(b);
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        };
        while (index < bytes.size() && isSpace(bytes[index]))
        {
            ++index;
        }

        if (index >= bytes.size())
        {
            return true;
        }

        const auto startsWith = [&](std::string_view literal) {
            if (bytes.size() - index < literal.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < literal.size(); ++i)
            {
                auto c = std::to_integer<unsigned char>(bytes[index + i]);
                if (c >= 'A' && c <= 'Z')
                {
                    c = static_cast<unsigned char>(c - 'A' + 'a');
                }
                if (c != static_cast<unsigned char>(literal[i]))
                {
                    return false;
                }
            }
            return true;
        };

        if (startsWith("<!doctype") || startsWith("<html") || startsWith("<head") || startsWith("<body") ||
            startsWith("<script") || startsWith("<?xml"))
        {
            return true;
        }

        const auto first = std::to_integer<unsigned char>(bytes[index]);
        if (first == '{' || first == '[')
        {
            return true;
        }

        for (std::size_t i = index; i < bytes.size(); ++i)
        {
            const auto c = std::to_integer<unsigned char>(bytes[i]);
            const bool printable = (c >= 0x20 && c <= 0x7E) || c == '\t' || c == '\r' || c == '\n';
            if (!printable)
            {
                return false;
            }
        }

        return true;
    }

}
