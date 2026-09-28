#pragma once

struct GLFWwindow;

class ImGuiManager {
  public:
    bool Initialize(GLFWwindow *window);

    void BeginFrame();
    void EndFrame();

    void Shutdown();
};