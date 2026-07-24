#include "TextureManager.h"

ID3D11Device *TextureManager::s_Device = nullptr;

std::unordered_map<int, Texture> TextureManager::s_Textures;

bool TextureManager::Initialize(ID3D11Device *device) {
    s_Device = device;

    return s_Device != nullptr;
}

void TextureManager::Shutdown() {
    s_Textures.clear();

    s_Device = nullptr;
}

Texture *TextureManager::Get(int resourceId) {
    auto it = s_Textures.find(resourceId);

    if (it != s_Textures.end())
        return &it->second;

    return Load(resourceId);
}

Texture *TextureManager::Load(int resourceId) {
    if (!s_Device)
        return nullptr;

    Texture texture;

    if (!texture.LoadFromResource(s_Device, resourceId))
        return nullptr;

    auto [it, inserted] = s_Textures.emplace(resourceId, std::move(texture));

    return &it->second;
}