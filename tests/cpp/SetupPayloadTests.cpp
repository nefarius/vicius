#include "SetupPayload.h"

#include <cstdio>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    int g_failed = 0;

    void Fail(const char* name, const std::string& detail)
    {
        std::fprintf(stderr, "FAIL %s: %s\n", name, detail.c_str());
        ++g_failed;
    }

    void ExpectEq(const char* name, const std::optional<std::string>& actual, const char* expected)
    {
        if (!actual.has_value())
        {
            Fail(name, "expected '" + std::string(expected) + "' got nullopt");
            return;
        }
        if (*actual != expected)
        {
            Fail(name, "expected '" + std::string(expected) + "' got '" + *actual + "'");
        }
    }

    void ExpectNone(const char* name, const std::optional<std::string>& actual)
    {
        if (actual.has_value())
        {
            Fail(name, "expected nullopt got '" + *actual + "'");
        }
    }

    void ExpectTrue(const char* name, bool actual)
    {
        if (!actual)
        {
            Fail(name, "expected true");
        }
    }

    void ExpectFalse(const char* name, bool actual)
    {
        if (actual)
        {
            Fail(name, "expected false");
        }
    }

    std::vector<std::byte> Bytes(std::string_view text)
    {
        std::vector<std::byte> out;
        out.reserve(text.size());
        for (const unsigned char c : text)
        {
            out.push_back(std::byte{c});
        }
        return out;
    }

    std::vector<std::byte> Bytes(std::initializer_list<unsigned char> raw)
    {
        std::vector<std::byte> out;
        out.reserve(raw.size());
        for (const unsigned char c : raw)
        {
            out.push_back(std::byte{c});
        }
        return out;
    }

    std::vector<std::byte> WithPrefix(std::initializer_list<unsigned char> prefix, std::string_view rest)
    {
        auto out = Bytes(prefix);
        for (const unsigned char c : rest)
        {
            out.push_back(std::byte{c});
        }
        return out;
    }
}

int main()
{
    using setuppayload::ExtensionFromUrl;
    using setuppayload::LooksLikeCompoundFile;
    using setuppayload::LooksLikeText;

    ExpectEq("msi url", ExtensionFromUrl("https://buildbot.example/builds/Vestigo/latest/Vestigo.msi"), "msi");
    ExpectEq("upper exe", ExtensionFromUrl("https://example.com/setup.EXE"), "exe");
    ExpectEq("query", ExtensionFromUrl("https://example.com/setup.EXE?token=abc"), "exe");
    ExpectEq("fragment", ExtensionFromUrl("https://example.com/file.msi#section"), "msi");
    ExpectEq("query and fragment", ExtensionFromUrl("https://example.com/file.Zip?x=1#y"), "zip");
    ExpectEq("single char", ExtensionFromUrl("https://example.com/file.a"), "a");
    ExpectEq("eight chars", ExtensionFromUrl("https://example.com/file.abcdefgh"), "abcdefgh");
    ExpectEq("dotfile", ExtensionFromUrl("https://example.com/.msi"), "msi");
    ExpectEq("backslash", ExtensionFromUrl("https://example.com/dir\\payload.MSI"), "msi");

    ExpectNone("no extension", ExtensionFromUrl("https://example.com/artifact"));
    ExpectNone("trailing slash", ExtensionFromUrl("https://example.com/dir/"));
    ExpectNone("empty extension", ExtensionFromUrl("https://example.com/file."));
    ExpectNone("nine chars", ExtensionFromUrl("https://example.com/file.abcdefghi"));
    ExpectNone("hyphen", ExtensionFromUrl("https://example.com/file.ms-i"));
    ExpectNone("space", ExtensionFromUrl("https://example.com/file.ms i"));
    ExpectNone("plus", ExtensionFromUrl("https://example.com/file.c++"));
    ExpectNone("traversal segment", ExtensionFromUrl("https://example.com/foo.msi/../../etc/passwd"));
    ExpectNone("dotdot", ExtensionFromUrl("https://example.com/foo.msi/.."));
    ExpectNone("query only dot", ExtensionFromUrl("https://example.com/file?name=foo.msi"));
    ExpectNone("empty", ExtensionFromUrl(""));

    const auto compound = Bytes({0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, 0x00, 0x00});
    const auto pe = Bytes({0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00});
    const auto zip = Bytes({0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00});
    const auto html = Bytes("<!DOCTYPE html><html><body>error</body></html>");
    const auto htmlUpper = Bytes("<HTML><BODY>nope</BODY></HTML>");
    const auto xml = Bytes("  <?xml version=\"1.0\"?><error/>");
    const auto json = Bytes("{\"error\":\"not found\"}");
    const auto jsonArray = Bytes("[1, 2, 3]");
    const auto plain = Bytes("this is a plain text error page");
    const auto bomHtml = WithPrefix({0xEF, 0xBB, 0xBF}, "<!DOCTYPE html>");
    const auto utf16 = Bytes({0xFF, 0xFE, 0x3C, 0x00});

    ExpectTrue("compound signature", LooksLikeCompoundFile(compound));
    ExpectFalse("compound truncated", LooksLikeCompoundFile(std::span<const std::byte>(compound).first(7)));
    ExpectFalse("pe is not compound", LooksLikeCompoundFile(pe));
    ExpectFalse("zip is not compound", LooksLikeCompoundFile(zip));
    ExpectFalse("html is not compound", LooksLikeCompoundFile(html));
    ExpectFalse("empty is not compound", LooksLikeCompoundFile(std::span<const std::byte>{}));

    ExpectTrue("html", LooksLikeText(html));
    ExpectTrue("html upper", LooksLikeText(htmlUpper));
    ExpectTrue("xml", LooksLikeText(xml));
    ExpectTrue("json", LooksLikeText(json));
    ExpectTrue("json array", LooksLikeText(jsonArray));
    ExpectTrue("plain", LooksLikeText(plain));
    ExpectTrue("utf8 bom html", LooksLikeText(bomHtml));
    ExpectTrue("utf16 bom", LooksLikeText(utf16));
    ExpectFalse("pe is not text", LooksLikeText(pe));
    ExpectFalse("zip is not text", LooksLikeText(zip));
    ExpectFalse("compound is not text", LooksLikeText(compound));
    ExpectFalse("empty is not text", LooksLikeText(std::span<const std::byte>{}));

    if (g_failed != 0)
    {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }

    std::puts("SetupPayloadTests passed");
    return 0;
}
