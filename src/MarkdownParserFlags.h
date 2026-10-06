#pragma once

#include "md4c.h"

namespace markdown
{
    /**
     * \brief MD4C dialect flags used by the changelog renderer.
     *
     * Keep this in one place so imgui_md and the parser tests cannot drift.
     * Bare HTTP(S) URLs are recognized via MD_FLAG_PERMISSIVEURLAUTOLINKS.
     */
    inline constexpr unsigned ParserFlags =
        MD_FLAG_TABLES | MD_FLAG_UNDERLINE | MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEURLAUTOLINKS;
}
