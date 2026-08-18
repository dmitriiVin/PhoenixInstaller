#include "InstallModePage.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"
#include "Widgets/Card.h"

#include <imgui.h>

InstallModePage::InstallModePage(InstallerContext &context) : m_Context(context) {
}

void InstallModePage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Способ установки");
    ImGui::PopFont();

    Layout::Space(20);

    ImGui::TextUnformatted("Выберите способ установки Windows");

    Layout::Space(20);

    //
    // Переустановка Windows
    //
    Card::Begin("mode_windows", m_Context.InstallMode == InstallMode::ReinstallWindows);

    ImGui::TextUnformatted("Переустановить Windows");
    ImGui::Spacing();
    ImGui::TextWrapped("Будет отформатирован только раздел Windows. "
                       "Разделы данных останутся без изменений.");

    if (Card::End())
        m_Context.InstallMode = InstallMode::ReinstallWindows;

    Layout::Space(15);

    //
    // Переустановка + Data
    //
    Card::Begin("mode_data", m_Context.InstallMode == InstallMode::ReinstallWindowsAndFormatData);

    ImGui::TextUnformatted("Переустановить Windows и раздел данных");
    ImGui::Spacing();
    ImGui::TextWrapped("Будут отформатированы раздел Windows и раздел данных.");

    if (Card::End())
        m_Context.InstallMode = InstallMode::ReinstallWindowsAndFormatData;

    Layout::Space(15);

    //
    // Полная очистка
    //
    Card::Begin("mode_clean", m_Context.InstallMode == InstallMode::CleanDisk);

    ImGui::TextUnformatted("Полностью очистить диск");
    ImGui::Spacing();
    ImGui::TextWrapped("Все разделы будут удалены. "
                       "Будет создана новая таблица разделов GPT.");

    if (Card::End())
        m_Context.InstallMode = InstallMode::CleanDisk;

    Layout::Space(30);

    ImGui::Separator();

    Layout::Space(15);

    constexpr float buttonWidth = 180.0f;
    constexpr float buttonHeight = 45.0f;

    if (ImGui::Button("Назад", ImVec2(buttonWidth, buttonHeight)))
        m_BackRequested = true;

    float right = ImGui::GetContentRegionAvail().x - buttonWidth;

    if (right > 0.0f)
        ImGui::SameLine(right + ImGui::GetCursorPosX());

    if (ImGui::Button("Далее", ImVec2(buttonWidth, buttonHeight)))
        m_NextRequested = true;

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool InstallModePage::BackRequested() const {
    return m_BackRequested;
}

bool InstallModePage::NextRequested() const {
    return m_NextRequested;
}

void InstallModePage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;
}