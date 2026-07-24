#pragma once

#include <unordered_map>

#include <d3d11.h>

#include "Texture.h"

class TextureManager {
  public:
    static bool Initialize(ID3D11Device *device);

    static void Shutdown();

    static Texture *Get(int resourceId);

  private:
    static Texture *Load(int resourceId);

  private:
    static ID3D11Device *s_Device;

    static std::unordered_map<int, Texture> s_Textures;
};