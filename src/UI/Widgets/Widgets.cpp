#include "Widgets.h"

#include "imgui.h"

namespace {
constexpr ImU32 BulletColor = IM_COL32(75, 171, 132, 255);
constexpr float BulletRadius = 4.5f;
constexpr float BulletSpacing = 18.0f;
} // namespace

void UI::Bullet(const char *text) {
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    ImVec2 cursor = ImGui::GetCursorScreenPos();

    float centerY = cursor.y + ImGui::GetTextLineHeight() * 0.5f;

    drawList->AddCircleFilled(ImVec2(cursor.x + BulletRadius, centerY), BulletRadius, BulletColor);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + BulletSpacing);

    ImGui::TextUnformatted(text);
}