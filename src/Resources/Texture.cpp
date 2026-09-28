#include "Texture.h"

#include "Utils/ResourceManager.h"

#include <GL/gl.h>
#include <stb_image.h>

Texture::~Texture() {
    Reset();
}

Texture::Texture(Texture &&other) noexcept {
    m_Texture = other.m_Texture;
    m_Width = other.m_Width;
    m_Height = other.m_Height;

    other.m_Texture = 0;
    other.m_Width = 0;
    other.m_Height = 0;
}

Texture &Texture::operator=(Texture &&other) noexcept {
    if (this != &other) {
        Reset();

        m_Texture = other.m_Texture;
        m_Width = other.m_Width;
        m_Height = other.m_Height;

        other.m_Texture = 0;
        other.m_Width = 0;
        other.m_Height = 0;
    }

    return *this;
}

void Texture::Reset() {
    if (m_Texture != 0) {
        GLuint texture = m_Texture;

        glDeleteTextures(1, &texture);

        m_Texture = 0;
    }

    m_Width = 0;
    m_Height = 0;
}

bool Texture::LoadFromFile(const std::filesystem::path &path) {
    Reset();

    auto data = ResourceManager::Load(path);

    if (data.empty())
        return false;

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc *pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
        return false;

    GLuint texture = 0;

    glGenTextures(1, &texture);

    if (texture == 0) {
        stbi_image_free(pixels);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    m_Texture = texture;
    m_Width = width;
    m_Height = height;

    return true;
}

bool Texture::IsValid() const {
    return m_Texture != 0;
}

std::uint32_t Texture::Get() const {
    return m_Texture;
}

int Texture::Width() const {
    return m_Width;
}

int Texture::Height() const {
    return m_Height;
}