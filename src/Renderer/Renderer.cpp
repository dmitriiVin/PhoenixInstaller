#include "Renderer.h"

#include <GL/gl.h>
#include <GLFW/glfw3.h>

bool Renderer::Initialize(GLFWwindow *window) {
    if (!window)
        return false;

    m_Window = window;

    glfwGetFramebufferSize(m_Window, reinterpret_cast<int *>(&m_Width), reinterpret_cast<int *>(&m_Height));

    glfwMakeContextCurrent(m_Window);

    return true;
}

void Renderer::BeginFrame() {
    if (!m_Window)
        return;

    glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));

    glClearColor(0.10f, 0.10f, 0.10f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndFrame() {
    if (!m_Window)
        return;

    glfwSwapBuffers(m_Window);
}

void Renderer::Resize(std::uint32_t width, std::uint32_t height) {
    m_Width = width;
    m_Height = height;

    if (m_Window) {
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
}

void Renderer::Shutdown() {
    m_Window = nullptr;
    m_Width = 0;
    m_Height = 0;
}