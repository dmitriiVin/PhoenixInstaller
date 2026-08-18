#include "Theme.h"

#include "imgui.h"

void Theme::Apply() {
    ImGuiStyle &style = ImGui::GetStyle();

    // Отступы
    style.WindowPadding = ImVec2(24.0f, 24.0f);
    style.FramePadding = ImVec2(18.0f, 12.0f);
    style.ItemSpacing = ImVec2(12.0f, 12.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

    // Скругления
    style.WindowRounding = 0.0f;
    style.ChildRounding = 14.0f;
    style.FrameRounding = 12.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 12.0f;
    style.TabRounding = 12.0f;

    // Границы
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;

    ImVec4 *colors = style.Colors;

    // Основной фон
    colors[ImGuiCol_WindowBg] = Background;
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);

    // Текст
    colors[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 0.98f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.67f, 0.70f, 0.76f, 1.0f);

    // Кнопки
    colors[ImGuiCol_Button] = Accent;
    colors[ImGuiCol_ButtonHovered] = AccentHovered;
    colors[ImGuiCol_ButtonActive] = AccentPressed;

    // Заголовки (используются многими виджетами ImGui)
    colors[ImGuiCol_Header] = Accent;
    colors[ImGuiCol_HeaderHovered] = AccentHovered;
    colors[ImGuiCol_HeaderActive] = AccentPressed;

    // Разделители
    colors[ImGuiCol_Separator] = ImVec4(43.0f / 255.0f, 50.0f / 255.0f, 64.0f / 255.0f, 1.0f);

    // Поля ввода
    colors[ImGuiCol_FrameBg] = ImVec4(28.0f / 255.0f, 32.0f / 255.0f, 45.0f / 255.0f, 1.0f);

    colors[ImGuiCol_FrameBgHovered] = ImVec4(36.0f / 255.0f, 41.0f / 255.0f, 58.0f / 255.0f, 1.0f);

    colors[ImGuiCol_FrameBgActive] = ImVec4(44.0f / 255.0f, 50.0f / 255.0f, 70.0f / 255.0f, 1.0f);

    // Скроллбар
    colors[ImGuiCol_ScrollbarBg] = ImVec4(18.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 1.0f);

    colors[ImGuiCol_ScrollbarGrab] = ImVec4(70.0f / 255.0f, 78.0f / 255.0f, 95.0f / 255.0f, 1.0f);

    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(90.0f / 255.0f, 100.0f / 255.0f, 120.0f / 255.0f, 1.0f);

    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(110.0f / 255.0f, 120.0f / 255.0f, 140.0f / 255.0f, 1.0f);

    // Без рамок
    colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
}