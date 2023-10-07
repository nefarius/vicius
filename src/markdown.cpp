#include "Updater.h"


ImFont* H1 = nullptr;
ImFont* H2 = nullptr;
ImFont* H3 = nullptr;

ImGui::MarkdownConfig mdConfig;


void LinkClickedCallback(ImGui::MarkdownLinkCallbackData data_)
{
	std::string url(data_.link, data_.linkLength);
	if (!data_.isImage)
	{
		ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
}

void FormatChangelogCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_)
{
	defaultMarkdownFormatCallback(markdownFormatInfo_, start_);

	switch (markdownFormatInfo_.type)
	{
	// example: change the colour of heading level 2
	case ImGui::MarkdownFormatType::HEADING:
		{
			if (markdownFormatInfo_.level == 2)
			{
				if (start_)
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

inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data_)
{
	// In your application you would load an image based on data_ input. Here we just use the imgui font texture.
	ImTextureID image = ImGui::GetIO().Fonts->TexID;
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

void RenderChangelog(const std::string& markdown)
{
	mdConfig.linkCallback = LinkClickedCallback;
	mdConfig.tooltipCallback = nullptr;
	mdConfig.imageCallback = ImageCallback;
	mdConfig.linkIcon = ICON_FA_LINK;
	mdConfig.headingFormats[0] = {H1, true};
	mdConfig.headingFormats[1] = {H2, true};
	mdConfig.headingFormats[2] = {H3, false};
	mdConfig.userData = nullptr;
	mdConfig.formatCallback = FormatChangelogCallback;
	Markdown(markdown.c_str(), markdown.length(), mdConfig);
}
