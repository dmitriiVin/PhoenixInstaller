#pragma once

#include <GLFW/glfw3.h>

class Window {
  public:
    bool Create(const char *title, int width, int height);

    bool ProcessMessages();

    GLFWwindow *GetHandle() const;

    void Destroy();

  private:
    GLFWwindow *m_Window = nullptr;
};