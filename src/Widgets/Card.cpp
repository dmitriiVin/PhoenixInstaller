#include "Card.h"

#include <imgui.h>

namespace {
constexpr float Radius = 10.0f;
constexpr float Padding = 16.0f;
} // namespace

bool Card::Begin(const char *id, float height, bool selected) {
    ImVec2 size(ImGui::GetContentRegionAvail().x, height);

    bool clicked = ImGui::InvisibleButton(id, size);

    bool hovered = ImGui::IsItemHovered();

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImU32 background = IM_COL32(38, 38, 38, 255);

    if (selected)
        background = IM_COL32(46, 112, 196, 255);
    else if (hovered)
        background = IM_COL32(55, 55, 55, 255);

    draw->AddRectFilled(min, max, background, Radius);
    draw->AddRect(min, max, IM_COL32(80, 80, 80, 255), Radius);

    ImGui::SetCursorScreenPos({min.x + Padding, min.y + Padding});

    ImGui::BeginGroup();

    return clicked;
}

void Card::End() {
    ImGui::EndGroup();
}