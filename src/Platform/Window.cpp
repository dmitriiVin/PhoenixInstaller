#include "Window.h"

bool Window::Create(const char *title, int width, int height) {
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (!m_Window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_Window);

    glfwSwapInterval(1);

    return true;
}

bool Window::ProcessMessages() {
    if (!m_Window)
        return false;

    glfwPollEvents();

    return !glfwWindowShouldClose(m_Window);
}

GLFWwindow *Window::GetHandle() const {
    return m_Window;
}

void Window::Destroy() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    glfwTerminate();
}