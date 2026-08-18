#include "SegmentedSlider.h"

#include "UI/Layout/Layout.h"
#include "UI/Theme/Theme.h"
#include <imgui_internal.h>

#include <algorithm>
#include <imgui.h>
#include <unordered_map>

namespace {
constexpr float Height = 14.0f;
constexpr float SegmentGap = 5.0f;
constexpr float GrabRadius = 11.0f;
constexpr int SegmentCount = 40;
} // namespace

int SegmentedSlider::Clamp(int value, int min, int max) {
    return std::clamp(value, min, max);
}

bool SegmentedSlider::Draw(const char *id, int *value, int min, int max, float width) {
    if (value == nullptr)
        return false;

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    if (window->SkipItems)
        return false;

    if (width <= 0.0f)
        width = ImGui::GetContentRegionAvail().x;

    const float sliderHeight = Layout::Scale(Height);

    const ImVec2 size(width, sliderHeight);

    ImGui::InvisibleButton(id, size);

    const ImVec2 minPos = ImGui::GetItemRectMin();
    const ImVec2 maxPos = ImGui::GetItemRectMax();

    ImDrawList *draw = ImGui::GetWindowDrawList();

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    bool changed = false;
    const float sliderWidth = maxPos.x - minPos.x;

    if (active && sliderWidth > 0.0f) {
        float t = (ImGui::GetIO().MousePos.x - minPos.x) / sliderWidth;

        t = std::clamp(t, 0.0f, 1.0f);

        int newValue = min + static_cast<int>((max - min) * t + 0.5f);

        newValue = Clamp(newValue, min, max);

        if (newValue != *value) {
            *value = newValue;
            changed = true;
        }
    }

    float t = 0.0f;

    if (max > min) {
        t = float(*value - min) / float(max - min);
    }
    t = std::clamp(t, 0.0f, 1.0f);

    const float radius = Layout::Scale(GrabRadius);
    const float segmentGap = Layout::Scale(SegmentGap);
    const float totalGap = (SegmentCount - 1) * segmentGap;
    const float segmentWidth = std::max(1.0f, (sliderWidth - totalGap) / SegmentCount);
    const float grabX = minPos.x + sliderWidth * t;
    const float centerY = (minPos.y + maxPos.y) * 0.5f;

    const ImU32 activeColor = ImGui::ColorConvertFloat4ToU32(Theme::Accent);

    const ImU32 inactiveColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

    static std::unordered_map<ImGuiID, float> animation;

    const ImGuiID sliderId = ImGui::GetID(id);

    float &animatedX = animation[sliderId];

    if (animatedX == 0.0f)
        animatedX = grabX;

    const float speed = 12.0f;
    const float alpha = std::min(ImGui::GetIO().DeltaTime * speed, 1.0f);

    animatedX = ImLerp(animatedX, grabX, alpha);

    const float grabPosition = animatedX;

    for (int i = 0; i < SegmentCount; i++) {
        float x = minPos.x + i * (segmentWidth + segmentGap);

        ImVec2 p1(x, minPos.y);
        ImVec2 p2(x + segmentWidth, maxPos.y);

        bool activeSegment = (x + segmentWidth * 0.5f) <= grabPosition;

        draw->AddRectFilled(p1, p2, activeSegment ? activeColor : inactiveColor, Layout::Scale(4.0f));
    }

    draw->AddCircleFilled(ImVec2(animatedX, centerY + Layout::Scale(1.0f)), radius + Layout::Scale(5.0f), IM_COL32(0, 0, 0, 45), 32);

    if (hovered || active) {
        ImVec4 glow = Theme::Accent;
        glow.w = 0.18f;
        draw->AddCircleFilled(ImVec2(animatedX, centerY), radius + Layout::Scale(5.0f), ImGui::ColorConvertFloat4ToU32(glow), 32);
    }

    float grabRadius = radius;

    if (hovered)
        grabRadius += Layout::Scale(1.5f);

    if (active)
        grabRadius += Layout::Scale(2.5f);

    draw->AddCircleFilled(ImVec2(animatedX, centerY), grabRadius, IM_COL32(255, 255, 255, 255), 32);

    draw->AddCircle(ImVec2(animatedX, centerY), grabRadius, IM_COL32(210, 210, 210, 255), 32, Layout::Scale(1.5f));

    draw->AddCircleFilled(ImVec2(animatedX, centerY), Layout::Scale(2.5f), activeColor, 16);

    return changed;
}