#include "Layout.h"

#include "imgui.h"

#include <algorithm>

namespace {
constexpr float BaseWidth = 1920.0f;
constexpr float BaseHeight = 1080.0f;

struct LayoutState {
    ImVec2 WindowSize{};
    float Scale = 1.0f;
};

LayoutState g_Layout;
} // namespace

void Layout::Begin() {
    ImGuiIO &io = ImGui::GetIO();

    g_Layout.WindowSize = io.DisplaySize;

    const float scaleX = Width() / BaseWidth;
    const float scaleY = Height() / BaseHeight;

    g_Layout.Scale = std::clamp(std::min(scaleX, scaleY), 0.70f, 2.50f);

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(g_Layout.WindowSize);

    ImGui::Begin("Root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
}

void Layout::End() {
    ImGui::End();
}

void Layout::BeginContent() {
    const float margin = Margin();

    ImGui::SetCursorPos({margin, margin});

    ImGui::BeginChild("Content", ImVec2(-margin, -margin), false, ImGuiWindowFlags_NoScrollbar);
}

void Layout::BeginContainer(float width) {
    const float containerWidth = Scale(width);

    float x = (Width() - containerWidth) * 0.5f;

    if (x < Margin())
        x = Margin();

    ImGui::SetCursorPosX(x);

    ImGui::BeginChild("Container", ImVec2(containerWidth, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
}

void Layout::EndContainer() {
    ImGui::EndChild();
}

void Layout::EndContent() {
    ImGui::EndChild();
}

float Layout::Scale(float value) {
    return value * g_Layout.Scale;
}

ImVec2 Layout::Scale(float x, float y) {
    return {Scale(x), Scale(y)};
}

float Layout::Width() {
    return g_Layout.WindowSize.x;
}

float Layout::Height() {
    return g_Layout.WindowSize.y;
}

float Layout::Margin() {
    return Scale(40.0f);
}

void Layout::Space(float pixels) {
    ImGui::Dummy(ImVec2(0.0f, Scale(pixels)));
}