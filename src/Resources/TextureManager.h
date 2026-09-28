#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "Texture.h"

class TextureManager {
  public:
    static bool Initialize();

    static void Shutdown();

    static Texture *Get(const std::filesystem::path &path);

  private:
    static Texture *Load(const std::filesystem::path &path);

  private:
    static bool s_Initialized;

    static std::unordered_map<std::string, Texture> s_Textures;
};