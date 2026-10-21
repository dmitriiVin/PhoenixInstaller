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
    Layout::BeginContainer(600.0f);

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Способ установки");
    ImGui::PopFont();

    Layout::Space(8);

    ImGui::TextUnformatted("Выберите способ установки Windows");

    Layout::Space(8);

    //
    // Переустановка Windows диск Д не трогаем
    //
    Card::Begin("mode_windows", m_Context.Mode == InstallMode::ReinstallWindows);

    ImGui::TextUnformatted("Переустановить Windows");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Будет отформатирован только раздел C: и установлена Windows заново. "
        "Раздел D: и AMDZ останутся без изменений."
    );

    if (Card::End())
        m_Context.Mode = InstallMode::ReinstallWindows;

    Layout::Space(5);

    //
    // стандартная установка
    //
    Card::Begin("mode_data", m_Context.Mode == InstallMode::ReinstallWindowsAndFormatData);

    ImGui::TextUnformatted("Полная очистка и переустановка Windows");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Раздел C: будет создан размером 120 ГБ, остальное доступное "
        "пространство будет отведено под D:. Данные на C: и D: будут удалены. "
        "Раздел AMDZ останется без изменений."
    );

    if (Card::End())
        m_Context.Mode = InstallMode::ReinstallWindowsAndFormatData;

    Layout::Space(5);

    //
    // Полная очистка + ручная разметка
    //
    Card::Begin("mode_clean", m_Context.Mode == InstallMode::CleanDisk);

    ImGui::TextUnformatted("Ручная разметка (для опытных инженеров)");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Выберите размер C:. Всё оставшееся доступное пространство "
        "будет отведено под D:. Раздел AMDZ останется без изменений."
    );

    if (Card::End())
        m_Context.Mode = InstallMode::CleanDisk;

    Layout::Space(8);

    ImGui::Separator();

    Layout::Space(5);

    constexpr float buttonWidth = 110.0f;
    constexpr float buttonHeight = 34.0f;

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