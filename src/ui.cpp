#include "pch.h"
#include "Common.h"
#include <imgui_internal.h>
#include <wincodec.h>


extern ImFont* G_Font_H1;
extern ImFont* G_Font_H2;
extern ImFont* G_Font_H3;
extern ImFont* G_Font_Default;


/**
 * \brief https://github.com/ocornut/imgui/issues/707#issuecomment-1372640066
 */
void ui::ApplyImGuiStyleDark(float scale)
{
    auto& colors = ImGui::GetStyle().Colors;
    colors[ ImGuiCol_WindowBg ] = ImVec4{0.1f, 0.1f, 0.13f, 1.0f};
    colors[ ImGuiCol_MenuBarBg ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Border
    colors[ ImGuiCol_Border ] = ImVec4{0.44f, 0.37f, 0.61f, 0.29f};
    colors[ ImGuiCol_BorderShadow ] = ImVec4{0.0f, 0.0f, 0.0f, 0.24f};

    // Text
    colors[ ImGuiCol_Text ] = ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
    colors[ ImGuiCol_TextDisabled ] = ImVec4{0.5f, 0.5f, 0.5f, 1.0f};

    // Headers
    colors[ ImGuiCol_Header ] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ ImGuiCol_HeaderHovered ] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ ImGuiCol_HeaderActive ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Buttons
    colors[ ImGuiCol_Button ] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ ImGuiCol_ButtonHovered ] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ ImGuiCol_ButtonActive ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_CheckMark ] = ImVec4{0.74f, 0.58f, 0.98f, 1.0f};

    // Popups
    colors[ ImGuiCol_PopupBg ] = ImVec4{0.1f, 0.1f, 0.13f, 0.92f};

    // Slider
    colors[ ImGuiCol_SliderGrab ] = ImVec4{0.44f, 0.37f, 0.61f, 0.54f};
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4{0.74f, 0.58f, 0.98f, 0.54f};

    // Frame BG
    colors[ ImGuiCol_FrameBg ] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ ImGuiCol_FrameBgActive ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Tabs
    colors[ ImGuiCol_Tab ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_TabHovered ] = ImVec4{0.24f, 0.24f, 0.32f, 1.0f};
    colors[ ImGuiCol_TabActive ] = ImVec4{0.2f, 0.22f, 0.27f, 1.0f};
    colors[ ImGuiCol_TabUnfocused ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Title
    colors[ ImGuiCol_TitleBg ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_TitleBgActive ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Scrollbar
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4{0.1f, 0.1f, 0.13f, 1.0f};
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4{0.24f, 0.24f, 0.32f, 1.0f};

    // Separator
    colors[ ImGuiCol_Separator ] = ImVec4{0.44f, 0.37f, 0.61f, 1.0f};
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4{0.74f, 0.58f, 0.98f, 1.0f};
    colors[ ImGuiCol_SeparatorActive ] = ImVec4{0.84f, 0.58f, 1.0f, 1.0f};

    // Resize Grip
    colors[ ImGuiCol_ResizeGrip ] = ImVec4{0.44f, 0.37f, 0.61f, 0.29f};
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4{0.74f, 0.58f, 0.98f, 0.29f};
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4{0.84f, 0.58f, 1.0f, 0.29f};

    auto& style = ImGui::GetStyle();
    style.TabRounding = 4;
    style.ScrollbarRounding = 9;
    style.WindowRounding = 7;
    style.GrabRounding = 3;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ChildRounding = 4;

    style.ScaleAllSizes(scale);
}

/**
 * \brief Loads fonts used in UI and Markdown widget from embedded resources.
 */
void ui::LoadFonts(HINSTANCE hInstance, const float sizePixels, float scale)
{
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    font_cfg.OversampleH = 1;
    font_cfg.OversampleV = 1;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Ruda bold
    const HRSRC ruda_bold_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_BOLD), RT_FONT);
    const int ruda_bold_size = static_cast<int>(SizeofResource(hInstance, ruda_bold_res));
    const LPVOID ruda_bold_data = LockResource(LoadResource(hInstance, ruda_bold_res));

    // Ruda Regular
    const HRSRC ruda_regular_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_REGULAR), RT_FONT);
    const int ruda_regular_size = static_cast<int>(SizeofResource(hInstance, ruda_regular_res));
    const LPVOID ruda_regular_data = LockResource(LoadResource(hInstance, ruda_regular_res));

    // Fork Awesome
    const HRSRC fk_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_FK), RT_FONT);
    const int fk_size = static_cast<int>(SizeofResource(hInstance, fk_res));
    const LPVOID fk_data = LockResource(LoadResource(hInstance, fk_res));

    // Base font
    io.Fonts->AddFontFromMemoryTTF(ruda_regular_data, ruda_regular_size, scale * sizePixels, &font_cfg);

    // Base font + Fork Awesome merged (default font)
    static constexpr ImWchar fk_range[ ] = {ICON_MIN_FK, ICON_MAX_FK, 0};
    ImFontConfig fk_cfg;
    fk_cfg.FontDataOwnedByAtlas = false;
    fk_cfg.MergeMode = true; // merge with default font
    fk_cfg.GlyphMinAdvanceX = scale * sizePixels;
    io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, scale * sizePixels, &fk_cfg, fk_range);

    // Base font + Fork Awesome + Emojis merged
    static ImWchar emj_range[ ] = {0x1, 0x1FFFF, 0};
    static ImFontConfig emj_cfg;
    emj_cfg.OversampleH = emj_cfg.OversampleV = 1;
    emj_cfg.MergeMode = true;
    emj_cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", scale * sizePixels, &emj_cfg, emj_range);

    // Bold headings H2
    io.Fonts->AddFontFromMemoryTTF(ruda_bold_data, ruda_bold_size, scale * sizePixels * 1.2f, &font_cfg);
    G_Font_H2 = io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, scale * sizePixels * 1.2f, &fk_cfg, fk_range);

    // Bold headings H3 (smaller)
    io.Fonts->AddFontFromMemoryTTF(ruda_bold_data, ruda_bold_size, scale * sizePixels * 1.0f, &font_cfg);
    G_Font_H3 = io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, scale * sizePixels * 1.0f, &fk_cfg, fk_range);

    // Bold headings H1
    io.Fonts->AddFontFromMemoryTTF(ruda_bold_data, ruda_bold_size, scale * sizePixels * 1.5f, &font_cfg);
    G_Font_H1 = io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, scale * sizePixels * 1.5f, &fk_cfg, fk_range);

    ImGui::SFML::UpdateFontTexture();
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
    RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), t0, t1, style.FrameRounding);
}

std::expected<void, HRESULT> ui::LoadTextureFromMemory(
    ID3D11Device* device,
    const uint8_t* imageData,
    size_t imageSize,
    ID3D11ShaderResourceView** srv
    )
{
    HRESULT hr = S_OK;
    IWICImagingFactory* wicFactory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11Texture2D* texture = nullptr;

    if (FAILED(hr = CoInitialize(nullptr))) return std::unexpected(hr);

    auto guard = sg::make_scope_guard(
        [texture, converter, frame, decoder, stream, wicFactory ]() noexcept
        {
            if (texture) texture->Release();
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (stream) stream->Release();
            if (wicFactory) wicFactory->Release();
            CoUninitialize();
        });

    if (FAILED(hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory)
    )))
        return std::unexpected(hr);

    if (FAILED(hr = wicFactory->CreateStream(&stream))) return std::unexpected(hr);
    if (FAILED(hr = stream->InitializeFromMemory((BYTE*)imageData, static_cast<DWORD>(imageSize)))) return std::unexpected(hr);

    if (FAILED(hr = wicFactory->CreateDecoderFromStream(
        stream,
        nullptr,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    )))
        return std::unexpected(hr);

    if (FAILED(hr = decoder->GetFrame(0, &frame))) return std::unexpected(hr);

    if (FAILED(hr = wicFactory->CreateFormatConverter(&converter))) return std::unexpected(hr);
    if (FAILED(hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    )))
        return std::unexpected(hr);

    UINT width, height;
    if (FAILED(hr = frame->GetSize(&width, &height))) return std::unexpected(hr);

    std::vector<uint8_t> pixelData(width * height * 4); // Assuming RGBA format
    if (FAILED(hr = converter->CopyPixels(
        nullptr,
        width * 4,
        static_cast<UINT>(pixelData.size()),
        pixelData.data()
    )))
        return std::unexpected(hr);

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixelData.data();
    initData.SysMemPitch = width * 4;

    if (FAILED(hr = device->CreateTexture2D(
        &textureDesc,
        &initData,
        &texture
    )))
        return std::unexpected(hr);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

    if (FAILED(hr = device->CreateShaderResourceView(texture, &srvDesc, srv))) return std::unexpected(hr);

    return {};
}
