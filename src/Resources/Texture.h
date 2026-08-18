#pragma once

#include <d3d11.h>

class Texture {
  public:
    Texture() = default;
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    bool LoadFromResource(ID3D11Device *device, int resourceId);

    void Reset();

    bool IsValid() const;

    ID3D11ShaderResourceView *Get() const;

    int Width() const;
    int Height() const;

  private:
    ID3D11ShaderResourceView *m_Texture = nullptr;

    int m_Width = 0;
    int m_Height = 0;
};