#pragma once


namespace markdown
{
    void RenderChangelog(const std::string& markdown);
}

namespace ui
{
    void ApplyImGuiStyleDark(float scale = 1.0f);
    void LoadFonts(HINSTANCE hInstance, float sizePixels = 16.0f, float scale = 1.0f);
    void IndeterminateProgressBar(const ImVec2& size_arg);
}
