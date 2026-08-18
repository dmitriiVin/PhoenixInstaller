#include "Card.h"

#include <imgui.h>

namespace {
constexpr float Radius = 10.0f;
constexpr float Padding = 16.0f;
} // namespace

void Card::Begin(const char *id, bool selected) {
    s_State.Selected = selected;

    ImGui::PushID(id);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Radius);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Padding, Padding));

    ImVec4 bg = selected ? ImVec4(0.18f, 0.44f, 0.77f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Border));

    ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
}

bool Card::End() {
    ImGui::EndChild();

    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    if (!s_State.Selected && hovered) {
        ImDrawList *draw = ImGui::GetWindowDrawList();

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();

        draw->AddRectFilled(min, max, IM_COL32(255, 255, 255, 18), Radius);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    ImGui::PopID();

    return clicked;
}