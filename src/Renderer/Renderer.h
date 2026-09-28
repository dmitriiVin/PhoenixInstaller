#pragma once

#include <cstdint>

struct GLFWwindow;

class Renderer {
  public:
    bool Initialize(GLFWwindow *window);

    void BeginFrame();
    void EndFrame();

    void Resize(std::uint32_t width, std::uint32_t height);

    void Shutdown();

  private:
    GLFWwindow *m_Window = nullptr;

    std::uint32_t m_Width = 0;
    std::uint32_t m_Height = 0;
};