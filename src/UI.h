#pragma once


namespace markdown
{
    void RenderChangelog(const std::string& markdown);
}

namespace ui
{
    enum class Theme { Dark, Light };

    /**
     * \brief Applies the Fluent-inspired ImGui palette and shape metrics for the given theme.
     *        Safe to call multiple times (e.g. on WM_DPICHANGED or WM_SETTINGCHANGE) without
     *        accumulating size scaling.
     */
    void ApplyImGuiStyle(Theme theme, float scale = 1.0f);

    void LoadFonts(HINSTANCE hInstance, float sizePixels = 16.0f, float scale = 1.0f);
    void IndeterminateProgressBar(const ImVec2& size_arg);
}
