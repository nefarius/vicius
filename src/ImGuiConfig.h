#pragma once

#include <memory>
#include <SFML/Graphics/Texture.hpp>

#define ImTextureID std::shared_ptr<sf::Texture>

namespace ImGui
{
    inline auto GetTextureVoidPtr(const ImTextureID& id) -> void*
    {
        return id.get();
    }
}
