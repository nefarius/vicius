#include "pch.h"
#include "imgui_md.h"

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <directxtk/WICTextureLoader.h>
#include <winrt/base.h>

using namespace DirectX;


// Fonts shared throughout app
ImFont* G_Font_H1 = nullptr;
ImFont* G_Font_H2 = nullptr;
ImFont* G_Font_H3 = nullptr;
ImFont* G_Font_Default = nullptr;

extern ID3D11Device* g_pd3dDevice;

struct D3DResource
{
    winrt::com_ptr<ID3D11Resource> resource;
    winrt::com_ptr<ID3D11ShaderResourceView> view;

    unsigned width;
    unsigned height;
};

using OptionalD3DResource = std::optional<D3DResource>;
using SharedFutureD3DResource = std::shared_future<OptionalD3DResource>;
using OptionalSharedFutureD3DResource = std::optional<SharedFutureD3DResource>;
using D3DResourceMap = std::map<std::string, OptionalD3DResource>;


// Mapping of image URLs -> DirectX textures
namespace
{
    D3DResourceMap G_ImageTextures;
}


/**
 * \brief Markdown parser and render widget.
 */
struct changelog : imgui_md
{
    std::map<std::string, OptionalD3DResource> _images;
    std::string m_img_href;
    std::regex m_regex_link_target;
    std::map<std::string /* URL */, OptionalSharedFutureD3DResource /* Texture downloader */> imageDownloadTasks;

    explicit changelog(const D3DResourceMap& images)
        : _images(images)
    {
        m_regex_link_target = std::regex(
            R"(\)\]\((https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*))\))",
            std::regex_constants::icase
            );
    }

    void SPAN_IMG(const MD_SPAN_IMG_DETAIL* d, bool e) override
    {
        m_is_image = e;

        m_img_href.assign(d->src.text, d->src.size);
        auto src = std::string(d->src.text + d->src.size);

        auto matchesBegin = std::sregex_iterator(src.begin(), src.end(), m_regex_link_target);
        auto matchesEnd = std::sregex_iterator();

        std::string real_h_ref;
        real_h_ref.assign(d->src.text, d->src.size);
        bool is_link = false;

        if (matchesBegin != matchesEnd)
        {
            if (const std::smatch& match = *matchesBegin; match.size() > 1)
            {
                real_h_ref.assign(match[ 1 ]);
                is_link = true;
            }
        }

        if (e)
        {
            image_info nfo;
            if (get_image(nfo))
            {
                const float scale = ImGui::GetIO().FontGlobalScale;
                nfo.size.x *= scale;
                nfo.size.y *= scale;

                const ImVec2 csz = ImGui::GetContentRegionAvail();
                if (nfo.size.x > csz.x)
                {
                    const float r = nfo.size.y / nfo.size.x;
                    nfo.size.x = csz.x;
                    nfo.size.y = csz.x * r;
                }

                ImGui::Image(nfo.texture_id, nfo.size, nfo.uv0, nfo.uv1, nfo.col_tint, nfo.col_border);

                if (ImGui::IsItemHovered())
                {
                    if (is_link)
                    {
                        ImGui::SetTooltip("%s", real_h_ref.c_str());
                    }

                    if (ImGui::IsMouseReleased(0))
                    {
                        ShellExecuteA(
                            nullptr,
                            "open",
                            real_h_ref.c_str(),
                            nullptr,
                            nullptr,
                            SW_SHOWNORMAL
                            );
                    }
                }
            }
        }
    }

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
    }

    ImVec4 get_color() const override
    {
        if (!m_href.empty())
        {
            // improve hyperlink visibility
            return ImGui::GetStyle().Colors[ ImGuiCol_CheckMark ];
        }

        return ImGui::GetStyle().Colors[ ImGuiCol_Text ];
    }

    void open_url() const override
    {
        ShellExecuteA(
            nullptr,
            "open",
            m_href.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
            );
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    OptionalD3DResource DownloadImageTexture(const std::string& url) const
    {
        const auto conn = new RestClient::Connection("");
        conn->FollowRedirects(true, 5);
        conn->SetTimeout(5);

        const RestClient::Response response = conn->get(url);

        if (response.code != httplib::OK_200) return std::nullopt;

        winrt::com_ptr<ID3D11Resource> txt;
        winrt::com_ptr<ID3D11ShaderResourceView> srv;

        winrt::init_apartment();

        HRESULT hr = CreateWICTextureFromMemory(
            g_pd3dDevice,
            reinterpret_cast<const uint8_t*>(response.body.data()),
            response.body.length(),
            txt.put(),
            srv.put()
            );

        return SUCCEEDED(hr)
                   ? std::make_optional<D3DResource>({txt, srv})
                   : std::nullopt;
    }

    /**
     * \brief Downloads a remote image resource and caches it in memory.
     * \param url The href of the image.
     * \return An OptionalD3DResource.
     */
    OptionalD3DResource GetImageTexture(const std::string& url)
    {
        // this gets called on every frame so we only download it once and cache it
        if (!_images.contains(url))
        {
            // no download task is assigned to the given url, initiate
            if (!imageDownloadTasks.contains(url) || !imageDownloadTasks[ url ].has_value())
            {
                const OptionalSharedFutureD3DResource downloadTask =
                    std::async(
                        std::launch::async,
                        &changelog::DownloadImageTexture,
                        this,
                        url
                        );
                imageDownloadTasks[ url ] = downloadTask;
            }
            // task is running or finished
            else if (imageDownloadTasks[ url ].has_value())
            {
                const OptionalSharedFutureD3DResource task = imageDownloadTasks[ url ];
                const std::future_status status = task->wait_for(std::chrono::milliseconds(1));

                const bool isDownloading = status == std::future_status::timeout;
                const bool hasFinished = status == std::future_status::ready;

                // no texture to report yet
                if (isDownloading) return std::nullopt;

                // task done (succeeded or failed)
                if (hasFinished)
                {
                    auto res = task->get();

                    // got our image, cache and return
                    if (res != std::nullopt)
                    {
                        auto [ width, height ] = GetTextureSize(res.value().view);

                        res.value().width = width;
                        res.value().height = height;

                        _images.emplace(url, res);
                        return res;
                    }

                    // retry on next frame
                    imageDownloadTasks[ url ].reset();
                }
            }

            return std::nullopt;
        }

        return _images[ url ];
    }

    static std::pair<uint32_t, uint32_t> GetTextureSize(const winrt::com_ptr<ID3D11ShaderResourceView>& srv)
    {
        if (!srv)
            return {0, 0};

        winrt::init_apartment();

        winrt::com_ptr<ID3D11Resource> resource;
        srv->GetResource(resource.put());

        const auto texture = resource.try_as<ID3D11Texture2D>();
        if (!texture)
            return {0, 0}; // Not a texture2D

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        return {desc.Width, desc.Height};
    }

    /**
     * \brief Pulls the texture for the requested image to render.
     * \param nfo Image info to fill out.
     * \return True on success, false otherwise.
     */
    bool get_image(image_info& nfo) const override
    {
        if (const OptionalD3DResource texture = const_cast<changelog*>(this)->GetImageTexture(m_img_href);
            texture.has_value())
        {
            const ImTextureID textureID = reinterpret_cast<ImTextureID>(texture.value().view.get());

            nfo.texture_id = textureID;
            nfo.size = {texture.value().width * 1.0f, texture.value().height * 1.0f};
            nfo.uv0 = {0, 0};
            nfo.uv1 = {1, 1};
            nfo.col_tint = {1, 1, 1, 1};
            nfo.col_border = {0, 0, 0, 0};
            return true;
        }

        return false;
    }
};

/**
 * \brief Parses a provided Markdown body string and renders it onto an ImGui widget.
 * \param markdown The Markdown document body.
 */
void markdown::RenderChangelog(const std::string& markdown)
{
    static changelog cl_render(G_ImageTextures);

    cl_render.print(markdown.c_str(), markdown.c_str() + markdown.length());
}
