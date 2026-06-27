#include "pch.h"
#include "Common.h"
#include "Util.h"
#include <imgui_internal.h>
#include <wincodec.h>


extern ImFont* G_Font_H1;
extern ImFont* G_Font_H2;
extern ImFont* G_Font_H3;
extern ImFont* G_Font_Default;
extern ImFont* G_Font_Bold;


/**
 * \brief Applies a Windows 11 Fluent-inspired dark theme.
 *        Safe to call multiple times (e.g. on WM_DPICHANGED) without accumulating scaling.
 */
void ui::ApplyImGuiStyleDark(float scale)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);

    const ImVec4 accent        = winapi::GetAccentColor();
    const ImVec4 accentHovered = winapi::GetAccentColorHovered();
    const ImVec4 accentActive  = winapi::GetAccentColorActive();

    // Fluent neutral dark surfaces (Fluent dark-mode layer tokens)
    // Base:        #202020   Card/child:  #272727   Control:   #2D2D2D
    // Subtle hover:#383838   Subtle press:#2A2A2A
    auto& c = style.Colors;

    // Surfaces
    c[ImGuiCol_WindowBg]          = ImVec4{0.125f, 0.125f, 0.125f, 1.000f}; // #202020
    c[ImGuiCol_ChildBg]           = ImVec4{0.153f, 0.153f, 0.153f, 1.000f}; // #272727
    c[ImGuiCol_PopupBg]           = ImVec4{0.153f, 0.153f, 0.153f, 0.980f};
    c[ImGuiCol_MenuBarBg]         = ImVec4{0.153f, 0.153f, 0.153f, 1.000f};

    // Borders — subtle white ~8 %
    c[ImGuiCol_Border]            = ImVec4{1.000f, 1.000f, 1.000f, 0.078f};
    c[ImGuiCol_BorderShadow]      = ImVec4{0.000f, 0.000f, 0.000f, 0.000f};

    // Text
    c[ImGuiCol_Text]              = ImVec4{1.000f, 1.000f, 1.000f, 1.000f};
    c[ImGuiCol_TextDisabled]      = ImVec4{1.000f, 1.000f, 1.000f, 0.361f}; // ~#FFFFFF5C

    // Frame / input backgrounds
    c[ImGuiCol_FrameBg]           = ImVec4{0.176f, 0.176f, 0.176f, 1.000f}; // #2D2D2D
    c[ImGuiCol_FrameBgHovered]    = ImVec4{0.220f, 0.220f, 0.220f, 1.000f};
    c[ImGuiCol_FrameBgActive]     = ImVec4{0.165f, 0.165f, 0.165f, 1.000f};

    // Title bar (hidden in this app, but set consistently)
    c[ImGuiCol_TitleBg]           = ImVec4{0.125f, 0.125f, 0.125f, 1.000f};
    c[ImGuiCol_TitleBgActive]     = ImVec4{0.153f, 0.153f, 0.153f, 1.000f};
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4{0.125f, 0.125f, 0.125f, 1.000f};

    // Buttons — Fluent "Subtle" button (neutral)
    c[ImGuiCol_Button]            = ImVec4{0.196f, 0.196f, 0.196f, 1.000f}; // #323232
    c[ImGuiCol_ButtonHovered]     = ImVec4{0.220f, 0.220f, 0.220f, 1.000f}; // #383838
    c[ImGuiCol_ButtonActive]      = ImVec4{0.165f, 0.165f, 0.165f, 1.000f}; // #2A2A2A

    // Headers (collapsibles, selectables)
    c[ImGuiCol_Header]            = ImVec4{0.196f, 0.196f, 0.196f, 1.000f};
    c[ImGuiCol_HeaderHovered]     = ImVec4{0.220f, 0.220f, 0.220f, 1.000f};
    c[ImGuiCol_HeaderActive]      = ImVec4{0.165f, 0.165f, 0.165f, 1.000f};

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]       = ImVec4{0.125f, 0.125f, 0.125f, 1.000f};
    c[ImGuiCol_ScrollbarGrab]     = ImVec4{1.000f, 1.000f, 1.000f, 0.200f};
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4{1.000f, 1.000f, 1.000f, 0.350f};
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4{1.000f, 1.000f, 1.000f, 0.500f};

    // Slider / grab
    c[ImGuiCol_SliderGrab]        = ImVec4{accent.x, accent.y, accent.z, 0.700f};
    c[ImGuiCol_SliderGrabActive]  = accent;

    // Checkmark / radio — accent
    c[ImGuiCol_CheckMark]         = accent;

    // Separator — subtle
    c[ImGuiCol_Separator]         = ImVec4{1.000f, 1.000f, 1.000f, 0.078f};
    c[ImGuiCol_SeparatorHovered]  = accentHovered;
    c[ImGuiCol_SeparatorActive]   = accent;

    // Resize grip — barely visible
    c[ImGuiCol_ResizeGrip]        = ImVec4{1.000f, 1.000f, 1.000f, 0.050f};
    c[ImGuiCol_ResizeGripHovered] = ImVec4{1.000f, 1.000f, 1.000f, 0.150f};
    c[ImGuiCol_ResizeGripActive]  = ImVec4{1.000f, 1.000f, 1.000f, 0.250f};

    // Tabs
    c[ImGuiCol_Tab]               = ImVec4{0.153f, 0.153f, 0.153f, 1.000f};
    c[ImGuiCol_TabHovered]        = ImVec4{0.220f, 0.220f, 0.220f, 1.000f};
    c[ImGuiCol_TabActive]         = ImVec4{0.196f, 0.196f, 0.196f, 1.000f};
    c[ImGuiCol_TabUnfocused]      = ImVec4{0.153f, 0.153f, 0.153f, 1.000f};
    c[ImGuiCol_TabUnfocusedActive]= ImVec4{0.176f, 0.176f, 0.176f, 1.000f};

    // Plot histogram (used by IndeterminateProgressBar fill)
    c[ImGuiCol_PlotHistogram]     = accent;
    c[ImGuiCol_PlotHistogramHovered] = accentHovered;

    // Fluent Win11 shape metrics
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 8.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.ScrollbarRounding = 4.0f;

    // Borders on frames give the Fluent "control stroke" appearance
    style.FrameBorderSize   = 1.0f;
    style.WindowBorderSize  = 1.0f;
    style.PopupBorderSize   = 1.0f;

    // Spacing / padding — slightly roomier than ImGui defaults
    style.WindowPadding  = ImVec2{16.0f, 16.0f};
    style.FramePadding   = ImVec2{12.0f,  6.0f};
    style.ItemSpacing    = ImVec2{ 8.0f,  8.0f};
    style.ItemInnerSpacing = ImVec2{6.0f, 6.0f};
    style.ScrollbarSize  = 12.0f;
    style.GrabMinSize    = 10.0f;

    style.ScaleAllSizes(scale);
}

/**
 * \brief Loads fonts used in UI and Markdown widget.
 *
 * Font priority for body (regular):
 *   1. Segoe UI Variable Text  (Win11, C:\Windows\Fonts\SegoeUIVariable-Text.ttf)
 *   2. Segoe UI                (Win10+, segoeui.ttf)
 *   3. Embedded Ruda Regular   (always available via resource)
 *
 * Font priority for bold/headings:
 *   1. Segoe UI Variable Display (Win11, SegoeUIVariable-Display.ttf)
 *   2. Segoe UI Bold             (segoeuib.ttf)
 *   3. Embedded Ruda Bold
 *
 * Icon (Fork Awesome) and Segoe UI Emoji glyphs are merged into every slot.
 * Safe to call multiple times (e.g. on WM_DPICHANGED).
 */
void ui::LoadFonts(HINSTANCE hInstance, const float sizePixels, float scale)
{
    namespace fs = std::filesystem;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // -- Segoe UI Variable paths (Win11) --
    const fs::path segoeVarText    = R"(C:\Windows\Fonts\SegoeUIVariable-Text.ttf)";
    const fs::path segoeVarDisplay = R"(C:\Windows\Fonts\SegoeUIVariable-Display.ttf)";
    // -- Classic Segoe UI paths (Win10+) --
    const fs::path segoeRegular    = R"(C:\Windows\Fonts\segoeui.ttf)";
    const fs::path segoeBold       = R"(C:\Windows\Fonts\segoeuib.ttf)";
    // -- Emoji --
    const fs::path emojiFontFile   = R"(C:\Windows\Fonts\seguiemj.ttf)";

    // Determine which regular/bold system font to use (highest-priority available)
    const bool hasSegoeVarText    = fs::is_regular_file(segoeVarText);
    const bool hasSegoeVarDisplay = fs::is_regular_file(segoeVarDisplay);
    const bool hasSegoeRegular    = fs::is_regular_file(segoeRegular);
    const bool hasSegoeBold       = fs::is_regular_file(segoeBold);

    const fs::path& regularFont = hasSegoeVarText    ? segoeVarText
                                : hasSegoeRegular    ? segoeRegular
                                : fs::path{};         // empty → fall back to embedded Ruda

    const fs::path& boldFont    = hasSegoeVarDisplay ? segoeVarDisplay
                                : hasSegoeBold       ? segoeBold
                                : fs::path{};

    spdlog::debug("LoadFonts: regular='{}' bold='{}'",
        regularFont.string(), boldFont.string());

    // Embedded Ruda resources (fallback, always present in the binary).
    // These are compile-time resources — a null handle or zero size means the
    // RC file was not compiled in, which is always a build defect.
    const HRSRC ruda_bold_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_BOLD), RT_FONT);
    _ASSERT(ruda_bold_res != nullptr);
    const HGLOBAL ruda_bold_global = LoadResource(hInstance, ruda_bold_res);
    _ASSERT(ruda_bold_global != nullptr);
    const int    ruda_bold_size = static_cast<int>(SizeofResource(hInstance, ruda_bold_res));
    _ASSERT(ruda_bold_size > 0);
    const LPVOID ruda_bold_data = LockResource(ruda_bold_global);
    _ASSERT(ruda_bold_data != nullptr);

    const HRSRC ruda_regular_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_REGULAR), RT_FONT);
    _ASSERT(ruda_regular_res != nullptr);
    const HGLOBAL ruda_regular_global = LoadResource(hInstance, ruda_regular_res);
    _ASSERT(ruda_regular_global != nullptr);
    const int    ruda_regular_size = static_cast<int>(SizeofResource(hInstance, ruda_regular_res));
    _ASSERT(ruda_regular_size > 0);
    const LPVOID ruda_regular_data = LockResource(ruda_regular_global);
    _ASSERT(ruda_regular_data != nullptr);

    // Fork Awesome (always embedded)
    const HRSRC fk_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_FK), RT_FONT);
    _ASSERT(fk_res != nullptr);
    const HGLOBAL fk_global = LoadResource(hInstance, fk_res);
    _ASSERT(fk_global != nullptr);
    const int    fk_size = static_cast<int>(SizeofResource(hInstance, fk_res));
    _ASSERT(fk_size > 0);
    const LPVOID fk_data = LockResource(fk_global);
    _ASSERT(fk_data != nullptr);

    // Shared font configs
    ImFontConfig base_cfg;
    base_cfg.FontDataOwnedByAtlas = false;
    base_cfg.OversampleH = 1;
    base_cfg.OversampleV = 1;

    static constexpr ImWchar fk_range[] = {ICON_MIN_FK, ICON_MAX_FK, 0};
    static ImWchar emj_range[] = {0x1, 0x1FFFF, 0};

    ImFontConfig fk_cfg;
    fk_cfg.FontDataOwnedByAtlas = false;
    fk_cfg.MergeMode = true;

    static ImFontConfig emj_cfg;
    emj_cfg.OversampleH = emj_cfg.OversampleV = 1;
    emj_cfg.MergeMode = true;
    emj_cfg.FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;

    // Helper: add a regular or bold font slot at a given size multiplier.
    // Loads from a system file path when available, otherwise falls back to
    // embedded Ruda memory data. Merges Fork Awesome icons and (optionally) emoji.
    // Returns the pointer of the *last* merged font added (the one ImGui uses for
    // this slot after merging).
    auto addFontSlot = [&](const fs::path& sysPath,
                           LPVOID           rudaData,
                           int              rudaSize,
                           float            sizeMul,
                           bool             isDefault) -> ImFont*
    {
        const float sz = scale * sizePixels * sizeMul;
        fk_cfg.GlyphMinAdvanceX = sz;

        ImFont* font = nullptr;
        if (!sysPath.empty())
        {
            font = io.Fonts->AddFontFromFileTTF(sysPath.string().c_str(), sz, &base_cfg);
        }
        if (!font)
        {
            font = io.Fonts->AddFontFromMemoryTTF(rudaData, rudaSize, sz, &base_cfg);
        }

        // Merge Fork Awesome icons
        io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, sz, &fk_cfg, fk_range);

        // Merge color emoji (optional — file may not exist)
        ImFont* last = font;
        if (fs::is_regular_file(emojiFontFile))
        {
            last = io.Fonts->AddFontFromFileTTF(emojiFontFile.string().c_str(), sz, &emj_cfg, emj_range);
        }

        (void)isDefault;
        return last ? last : font;
    };

    // Default body font (regular, 1×)
    G_Font_Default = addFontSlot(regularFont, ruda_regular_data, ruda_regular_size, 1.0f, true);

    // Bold font (1×)
    G_Font_Bold = addFontSlot(boldFont, ruda_bold_data, ruda_bold_size, 1.0f, false);

    // Heading H3 = bold 1.0× (same size as body, but bold face)
    G_Font_H3 = addFontSlot(boldFont, ruda_bold_data, ruda_bold_size, 1.0f, false);

    // Heading H2 = bold 1.2×
    G_Font_H2 = addFontSlot(boldFont, ruda_bold_data, ruda_bold_size, 1.2f, false);

    // Heading H1 = bold 1.5×
    G_Font_H1 = addFontSlot(boldFont, ruda_bold_data, ruda_bold_size, 1.5f, false);

    io.FontDefault = G_Font_Default;
}

/**
 * \brief https://github.com/ocornut/imgui/issues/5370#issuecomment-1145917633
 */
void ui::IndeterminateProgressBar(const ImVec2& size_arg)
{
    using namespace ImGui;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiStyle& style = g.Style;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    ItemSize(size);
    if (!ItemAdd(bb, 0)) return;

    const float speed = g.FontSize * 0.05f;
    const float phase = ImFmod((float)g.Time * speed, 1.0f);
    const float width_normalized = 0.2f;
    float t0 = phase * (1.0f + width_normalized) - width_normalized;
    float t1 = t0 + width_normalized;

    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    {
        ImVec2 fill_min(ImLerp(bb.Min.x, bb.Max.x, t0), bb.Min.y);
        ImVec2 fill_max(ImLerp(bb.Min.x, bb.Max.x, t1), bb.Max.y);
        window->DrawList->PushClipRect(bb.Min, bb.Max, true);
        window->DrawList->AddRectFilled(fill_min, fill_max, GetColorU32(ImGuiCol_PlotHistogram), style.FrameRounding);
        window->DrawList->PopClipRect();
    }
}
