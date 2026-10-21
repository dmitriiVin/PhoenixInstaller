#include "ImGuiManager.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <algorithm>

namespace {
ImGuiStyle g_BaseStyle;
bool g_BaseStyleSaved = false;
}

bool ImGuiManager::Initialize(GLFWwindow *window) {
    if (!window)
        return false;

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    g_BaseStyle = ImGui::GetStyle();
    g_BaseStyleSaved = true;

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
        return false;

    if (!ImGui_ImplOpenGL3_Init("#version 330"))
        return false;

    return true;
}

void ImGuiManager::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // Adapt spacing and controls to small screens without making fonts tiny.
    ImGuiIO &io = ImGui::GetIO();
    const float widthScale = io.DisplaySize.x > 0.0f ? io.DisplaySize.x / 960.0f : 1.0f;
    const float heightScale = io.DisplaySize.y > 0.0f ? io.DisplaySize.y / 720.0f : 1.0f;
    const float uiScale = std::clamp(std::min(widthScale, heightScale), 0.68f, 1.0f);

    if (g_BaseStyleSaved) {
        ImGui::GetStyle() = g_BaseStyle;
        ImGui::GetStyle().ScaleAllSizes(uiScale);
    }

    // Keep text comfortably readable at 640x480; shrink only slightly.
    io.FontGlobalScale = (uiScale < 0.8f) ? 0.92f : 1.0f;

    ImGui::NewFrame();
}

void ImGuiManager::EndFrame() {
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();

    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();
}