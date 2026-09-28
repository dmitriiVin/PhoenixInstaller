#include "TextureManager.h"

bool TextureManager::s_Initialized = false;

std::unordered_map<std::string, Texture> TextureManager::s_Textures;

bool TextureManager::Initialize() {
    s_Initialized = true;

    return true;
}

void TextureManager::Shutdown() {
    s_Textures.clear();

    s_Initialized = false;
}

Texture *TextureManager::Get(const std::filesystem::path &path) {
    const std::string key = path.lexically_normal().string();

    auto it = s_Textures.find(key);

    if (it != s_Textures.end())
        return &it->second;

    return Load(path);
}

Texture *TextureManager::Load(const std::filesystem::path &path) {
    if (!s_Initialized)
        return nullptr;

    Texture texture;

    if (!texture.LoadFromFile(path))
        return nullptr;

    const std::string key = path.lexically_normal().string();

    auto [it, inserted] = s_Textures.emplace(key, std::move(texture));

    (void)inserted;

    return &it->second;
}