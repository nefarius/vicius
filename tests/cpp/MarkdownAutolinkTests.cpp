#include "MarkdownParserFlags.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    struct Link
    {
        std::string href;
        bool autolink = false;
    };

    struct ParseResult
    {
        std::vector<Link> links;
    };

    int EnterBlock(MD_BLOCKTYPE, void*, void*)
    {
        return 0;
    }

    int LeaveBlock(MD_BLOCKTYPE, void*, void*)
    {
        return 0;
    }

    int LeaveSpan(MD_SPANTYPE, void*, void*)
    {
        return 0;
    }

    int Text(MD_TEXTTYPE, const MD_CHAR*, MD_SIZE, void*)
    {
        return 0;
    }

    int EnterSpan(MD_SPANTYPE type, void* detail, void* userdata)
    {
        if (type != MD_SPAN_A)
            return 0;

        auto& result = *static_cast<ParseResult*>(userdata);
        const auto* link = static_cast<const MD_SPAN_A_DETAIL*>(detail);
        result.links.push_back({
            std::string(link->href.text, link->href.size),
            link->is_autolink != 0
        });
        return 0;
    }

    ParseResult Parse(const std::string& markdown)
    {
        ParseResult result;
        MD_PARSER parser{};
        parser.flags = markdown::ParserFlags;
        parser.enter_block = EnterBlock;
        parser.leave_block = LeaveBlock;
        parser.enter_span = EnterSpan;
        parser.leave_span = LeaveSpan;
        parser.text = Text;
        md_parse(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), &parser, &result);
        return result;
    }

    int g_failed = 0;

    void ExpectEq(const char* name, const std::string& actual, const char* expected)
    {
        if (actual != expected)
        {
            std::fprintf(stderr, "FAIL %s: expected '%s' got '%s'\n", name, expected, actual.c_str());
            ++g_failed;
        }
    }

    void ExpectEq(const char* name, size_t actual, size_t expected)
    {
        if (actual != expected)
        {
            std::fprintf(stderr, "FAIL %s: expected %zu got %zu\n", name, expected, actual);
            ++g_failed;
        }
    }

    void ExpectTrue(const char* name, bool actual)
    {
        if (!actual)
        {
            std::fprintf(stderr, "FAIL %s\n", name);
            ++g_failed;
        }
    }
}

int main()
{
    const char* githubBody =
        "Full changelog: https://github.com/nefarius/BthPS3/compare/setup-v3.0.0-r6...setup-v3.2.0\n"
        "\n"
        "* diag by @nefarius in https://github.com/nefarius/BthPS3/pull/177\n"
        "* explicit [label](https://example.com/md-link)\n"
        "* already wrapped <https://example.com/angle>\n"
        "* keep code `https://example.com/code` as text\n";

    const ParseResult parsed = Parse(githubBody);
    ExpectEq("github body link count", parsed.links.size(), 4);
    ExpectEq("compare href", parsed.links.size() > 0 ? parsed.links[0].href : "",
             "https://github.com/nefarius/BthPS3/compare/setup-v3.0.0-r6");
    ExpectTrue("compare is autolink", parsed.links.size() > 0 && parsed.links[0].autolink);
    ExpectEq("pull href", parsed.links.size() > 1 ? parsed.links[1].href : "",
             "https://github.com/nefarius/BthPS3/pull/177");
    ExpectTrue("pull is autolink", parsed.links.size() > 1 && parsed.links[1].autolink);
    ExpectEq("markdown href", parsed.links.size() > 2 ? parsed.links[2].href : "",
             "https://example.com/md-link");
    ExpectTrue("markdown link is not autolink", parsed.links.size() > 2 && !parsed.links[2].autolink);
    ExpectEq("angle href", parsed.links.size() > 3 ? parsed.links[3].href : "",
             "https://example.com/angle");
    ExpectTrue("angle is autolink", parsed.links.size() > 3 && parsed.links[3].autolink);

    const ParseResult angleCompare = Parse(
        "<https://github.com/nefarius/BthPS3/compare/setup-v3.0.0-r6...setup-v3.2.0>");
    ExpectEq("angle compare link count", angleCompare.links.size(), 1);
    ExpectEq("angle compare href", angleCompare.links.size() > 0 ? angleCompare.links[0].href : "",
             "https://github.com/nefarius/BthPS3/compare/setup-v3.0.0-r6...setup-v3.2.0");

    const ParseResult plain = Parse("Ordinary text with no URLs.");
    ExpectEq("plain text link count", plain.links.size(), 0);

    if (g_failed)
    {
        std::fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }

    std::puts("MarkdownAutolinkTests: OK");
    return 0;
}
