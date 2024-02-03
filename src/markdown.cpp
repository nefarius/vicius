#include "pch.h"
#include "Common.h"
#include "imgui_md.h"


ImFont* G_Font_H1 = nullptr;
ImFont* G_Font_H2 = nullptr;
ImFont* G_Font_H3 = nullptr;
ImFont* G_Font_Default = nullptr;

ImGui::MarkdownConfig mdConfig;


#if 0
// ReSharper disable once CppPassValueParameterByConstReference
static void LinkClickedCallback(ImGui::MarkdownLinkCallbackData data)
{
    const std::string url(data.link, data.linkLength);
	if (!data.isImage)
	{
		ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
}

// ReSharper disable once CppParameterMayBeConst
static void FormatChangelogCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo, bool start)
{
	defaultMarkdownFormatCallback(markdownFormatInfo, start);

	switch (markdownFormatInfo.type)
	{
	// example: change the colour of heading level 2
	case ImGui::MarkdownFormatType::HEADING:
		{
			if (markdownFormatInfo.level == 2)
			{
				if (start)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				}
				else
				{
					ImGui::PopStyleColor();
				}
			}
			break;
		}
	default:
		{
			break;
		}
	}
}

inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data)
{
	// In your application you would load an image based on data_ input. Here we just use the imgui font texture.
    const ImTextureID image = ImGui::GetIO().Fonts->TexID;
	// > C++14 can use ImGui::MarkdownImageData imageData{ true, false, image, ImVec2( 40.0f, 20.0f ) };
	ImGui::MarkdownImageData imageData;
	imageData.isValid = true;
	imageData.useLinkCallback = false;
	imageData.user_texture_id = image;
	imageData.size = ImVec2(40.0f, 20.0f);

	// For image resize when available size.x > image width, add
	const ImVec2 contentSize = ImGui::GetContentRegionAvail();
	if (imageData.size.x > contentSize.x)
	{
		const float ratio = imageData.size.y / imageData.size.x;
		imageData.size.x = contentSize.x;
		imageData.size.y = contentSize.x * ratio;
	}

	return imageData;
}
#endif

struct changelog : public imgui_md
{
    ImFont* get_font() const override
    {
        switch (m_hlevel)
        {
        case 1:
            return G_Font_H1;
        case 2:
            return G_Font_H2;
        case 3:
            return G_Font_H3;
        default:
        case 0:
            return G_Font_Default;
        }
    };

    void open_url() const override
    {
        ShellExecuteA(nullptr, "open", m_href.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    bool get_image(image_info& nfo) const override
    {
        //use m_href to identify images
        //nfo.texture_id = g_texture1;
        nfo.size = {40, 20};
        nfo.uv0 = {0, 0};
        nfo.uv1 = {1, 1};
        nfo.col_tint = {1, 1, 1, 1};
        nfo.col_border = {0, 0, 0, 0};
        return true;
    }

    void html_div(const std::string& dclass, bool e) override
    {
        if (dclass == "red")
        {
            if (e)
            {
                m_table_border = false;
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            }
            else
            {
                ImGui::PopStyleColor();
                m_table_border = true;
            }
        }
    }
};

#if 0
void markdown::RenderChangelog(const std::string& markdown)
{
	mdConfig.linkCallback = LinkClickedCallback;
	mdConfig.tooltipCallback = nullptr;
	mdConfig.imageCallback = ImageCallback;
	mdConfig.linkIcon = ICON_FK_LINK;
	mdConfig.headingFormats[0] = {G_Font_H1, false};
	mdConfig.headingFormats[1] = {G_Font_H2, false};
	mdConfig.headingFormats[2] = {G_Font_H3, false};
	mdConfig.userData = nullptr;
	mdConfig.formatCallback = FormatChangelogCallback;
	Markdown(markdown.c_str(), markdown.length(), mdConfig);
}
#else
void markdown::RenderChangelog(const std::string& markdown)
{
    static changelog cl_render;

    cl_render.print(markdown.c_str(), markdown.c_str() + markdown.length());
}
#endif
