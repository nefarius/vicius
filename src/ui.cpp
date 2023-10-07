#include "Updater.h"


extern ImFont* H1;
extern ImFont* H2;
extern ImFont* H3;

extern ImGui::MarkdownConfig mdConfig;

/**
 * \brief https://github.com/ocornut/imgui/issues/707#issuecomment-1372640066
 */
void ApplyImGuiStyleDark()
{
	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
	colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

	// Border
	colors[ImGuiCol_Border] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
	colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f };

	// Text
	colors[ImGuiCol_Text] = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
	colors[ImGuiCol_TextDisabled] = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };

	// Headers
	colors[ImGuiCol_Header] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_HeaderActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

	// Buttons
	colors[ImGuiCol_Button] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_ButtonActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_CheckMark] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };

	// Popups
	colors[ImGuiCol_PopupBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 0.92f };

	// Slider
	colors[ImGuiCol_SliderGrab] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f };
	colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f };

	// Frame BG
	colors[ImGuiCol_FrameBg] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TabHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
	colors[ImGuiCol_TabActive] = ImVec4{ 0.2f, 0.22f, 0.27f, 1.0f };
	colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

	// Scrollbar
	colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
	colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };

	// Separator
	colors[ImGuiCol_Separator] = ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f };
	colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
	colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 1.0f };

	// Resize Grip
	colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
	colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.29f };
	colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 0.29f };

	auto& style = ImGui::GetStyle();
	style.TabRounding = 4;
	style.ScrollbarRounding = 9;
	style.WindowRounding = 7;
	style.GrabRounding = 3;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ChildRounding = 4;
}

void LoadFonts(HINSTANCE hInstance, const float sizePixels)
{
	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;

	const ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	// get embedded fonts
	const HRSRC ruda_bold_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_BOLD), RT_FONT);
	const int ruda_bold_size = static_cast<int>(SizeofResource(hInstance, ruda_bold_res));
	const LPVOID ruda_bold_data = LockResource(LoadResource(hInstance, ruda_bold_res));

	const HRSRC ruda_regular_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_RUDA_REGULAR), RT_FONT);
	const int ruda_regular_size = static_cast<int>(SizeofResource(hInstance, ruda_regular_res));
	const LPVOID ruda_regular_data = LockResource(LoadResource(hInstance, ruda_regular_res));

	// Fork Awesome
	const HRSRC fk_res = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_FK), RT_FONT);
	const int fk_size = static_cast<int>(SizeofResource(hInstance, fk_res));
	const LPVOID fk_data = LockResource(LoadResource(hInstance, fk_res));

	// Base font
	io.Fonts->AddFontFromMemoryTTF(ruda_regular_data, ruda_regular_size, sizePixels, &font_cfg);

	// Fork Awesome
	ImFontConfig far_cfg;
	far_cfg.FontDataOwnedByAtlas = false;
	far_cfg.MergeMode = true; // merge with default font
	far_cfg.GlyphMinAdvanceX = sizePixels;
	static const ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
	io.Fonts->AddFontFromMemoryTTF(fk_data, fk_size, sizePixels, &far_cfg, icon_ranges);

	// Bold headings H2 and H3
	H2 = io.Fonts->AddFontFromMemoryTTF(ruda_bold_data, ruda_bold_size, sizePixels, &font_cfg);
	H3 = mdConfig.headingFormats[1].font;
	// bold heading H1
	const float pixelsH1 = sizePixels * 1.1f;
	H1 = io.Fonts->AddFontFromMemoryTTF(ruda_bold_data, ruda_bold_size, pixelsH1, &font_cfg);
	
	ImGui::SFML::UpdateFontTexture();
}

