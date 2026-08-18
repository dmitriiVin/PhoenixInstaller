#include "ValueStepper.h"

#include "UI/Fonts/Fonts.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <unordered_map>

#include <algorithm>
#include <cstdio>

namespace {
struct StepperAnim {
    float MinusHover = 0.0f;
    float PlusHover = 0.0f;
};

static std::unordered_map<ImGuiID, StepperAnim> g_Animations;

constexpr float Radius = 8.0f;

ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
    ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);

    ImVec4 c;
    c.x = ImLerp(ca.x, cb.x, t);
    c.y = ImLerp(ca.y, cb.y, t);
    c.z = ImLerp(ca.z, cb.z, t);
    c.w = ImLerp(ca.w, cb.w, t);

    return ImGui::ColorConvertFloat4ToU32(c);
}
} // namespace

bool ValueStepper::Draw(const char *id, int *value, int min, int max, float width, float height) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();

    if (window->SkipItems)
        return false;

    ImGuiContext &g = *GImGui;

    ImGuiID widgetId = window->GetID(id);

    ImVec2 start = ImGui::GetCursorScreenPos();

    ImRect totalRect(start, ImVec2(start.x + width, start.y + height));

    ImGui::ItemSize(totalRect);
    if (!ImGui::ItemAdd(totalRect, widgetId))
        return false;

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d ГБ", *value);

    ImGui::PushFont(Fonts::Title());

    ImVec2 textSize = ImGui::CalcTextSize(buffer);

    ImGui::PopFont();

    const float buttonSize = height;
    const float gap = 12.0f;

    // Общая ширина группы [-] 100 ГБ [+]
    const float groupWidth = buttonSize + gap + textSize.x + gap + buttonSize;

    // Начало группы (по центру виджета)
    const float startX = totalRect.Min.x + (width - groupWidth) * 0.5f;

    // Кнопка "-"
    ImRect minusRect(ImVec2(startX, totalRect.Min.y), ImVec2(startX + buttonSize, totalRect.Min.y + buttonSize));

    // Текст
    ImRect textRect(ImVec2(minusRect.Max.x + gap, totalRect.Min.y), ImVec2(minusRect.Max.x + gap + textSize.x, totalRect.Max.y));

    // Кнопка "+"
    ImRect plusRect(ImVec2(textRect.Max.x + gap, totalRect.Min.y), ImVec2(textRect.Max.x + gap + buttonSize, totalRect.Max.y));

    bool changed = false;

    ImGui::SetCursorScreenPos(minusRect.Min);
    ImGui::InvisibleButton("##minus", minusRect.GetSize());

    bool minusHovered = ImGui::IsItemHovered();
    bool minusHeld = ImGui::IsItemActive();
    bool minusPressed = ImGui::IsItemClicked();

    ImGui::SetCursorScreenPos(plusRect.Min);
    ImGui::InvisibleButton("##plus", plusRect.GetSize());

    bool plusHovered = ImGui::IsItemHovered();
    bool plusHeld = ImGui::IsItemActive();
    bool plusPressed = ImGui::IsItemClicked();

    auto &anim = g_Animations[widgetId];

    float speed = g.IO.DeltaTime * 12.0f;

    anim.MinusHover = ImLerp(anim.MinusHover, minusHovered ? 1.0f : 0.0f, speed);

    anim.PlusHover = ImLerp(anim.PlusHover, plusHovered ? 1.0f : 0.0f, speed);

    ImDrawList *draw = ImGui::GetWindowDrawList();

    const ImU32 buttonColor = ImGui::GetColorU32(ImGuiCol_Button);

    const ImU32 buttonHoverColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

    const ImU32 buttonActiveColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);

    ImU32 minusColor = LerpColor(buttonColor, buttonHoverColor, anim.MinusHover);

    ImU32 plusColor = LerpColor(buttonColor, buttonHoverColor, anim.PlusHover);

    if (minusHeld)
        minusColor = buttonActiveColor;

    if (plusHeld)
        plusColor = buttonActiveColor;

    draw->AddRectFilled(minusRect.Min, minusRect.Max, minusColor, Radius);

    draw->AddRectFilled(plusRect.Min, plusRect.Max, plusColor, Radius);

    {
        ImVec2 size = ImGui::CalcTextSize("-");

        ImVec2 pos(minusRect.GetCenter().x - size.x * 0.5f, minusRect.GetCenter().y - size.y * 0.5f);

        draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), "-");
    }

    {
        ImVec2 size = ImGui::CalcTextSize("+");

        ImVec2 pos(plusRect.GetCenter().x - size.x * 0.5f, plusRect.GetCenter().y - size.y * 0.5f);

        draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), "+");
    }

    ImGui::PushFont(Fonts::Title());

    draw->AddText(ImVec2(textRect.Min.x, textRect.GetCenter().y - textSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), buffer);

    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(totalRect.Min.x, totalRect.Max.y));

    if (minusPressed && *value > min) {
        --(*value);
        changed = true;
    }

    if (plusPressed && *value < max) {
        ++(*value);
        changed = true;
    }

    return changed;
}