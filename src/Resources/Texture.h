#pragma once

#include <cstdint>
#include <filesystem>

class Texture {
  public:
    Texture() = default;
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    bool LoadFromFile(const std::filesystem::path &path);

    void Reset();

    bool IsValid() const;

    std::uint32_t Get() const;

    int Width() const;
    int Height() const;

  private:
    std::uint32_t m_Texture = 0;

    int m_Width = 0;
    int m_Height = 0;
};