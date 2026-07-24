// Texture.cpp
#include "Texture.h"

#include "Utils/ResourceManager.h"

#include <stb_image.h>

Texture::~Texture() {
    Reset();
}

Texture::Texture(Texture &&other) noexcept {
    m_Texture = other.m_Texture;
    m_Width = other.m_Width;
    m_Height = other.m_Height;

    other.m_Texture = nullptr;
    other.m_Width = 0;
    other.m_Height = 0;
}

Texture &Texture::operator=(Texture &&other) noexcept {
    if (this != &other) {
        Reset();

        m_Texture = other.m_Texture;
        m_Width = other.m_Width;
        m_Height = other.m_Height;

        other.m_Texture = nullptr;
        other.m_Width = 0;
        other.m_Height = 0;
    }

    return *this;
}

void Texture::Reset() {
    if (m_Texture) {
        m_Texture->Release();
        m_Texture = nullptr;
    }

    m_Width = 0;
    m_Height = 0;
}

bool Texture::LoadFromResource(ID3D11Device *device, int resourceId) {
    Reset();

    auto data = ResourceManager::Load(resourceId);
    if (data.empty())
        return false;

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc *pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
        return false;

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = pixels;
    subresource.SysMemPitch = width * 4;
    subresource.SysMemSlicePitch = 0;

    ID3D11Texture2D *texture = nullptr;

    HRESULT hr = device->CreateTexture2D(&textureDesc, &subresource, &texture);

    if (FAILED(hr)) {
        stbi_image_free(pixels);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(texture, &srvDesc, &m_Texture);

    texture->Release();

    stbi_image_free(pixels);

    if (FAILED(hr)) {
        Reset();
        return false;
    }

    m_Width = width;
    m_Height = height;

    return true;
}

bool Texture::IsValid() const {
    return m_Texture != nullptr;
}

ID3D11ShaderResourceView *Texture::Get() const {
    return m_Texture;
}

int Texture::Width() const {
    return m_Width;
}

int Texture::Height() const {
    return m_Height;
}
