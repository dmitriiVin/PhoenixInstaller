#include "InstallPage.h"

#include "Core/DiskManager.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"
#include "Widgets/DiskCard.h"

#include <imgui.h>

InstallPage::InstallPage(InstallerContext &context) : m_Context(context) {
    m_Disks = DiskManager::Enumerate();
}

void InstallPage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Установка Windows");
    ImGui::PopFont();

    Layout::Space(20);

    ImGui::TextUnformatted("Выберите диск для установки Windows");

    Layout::Space(20);

    if (m_Disks.empty()) {
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Физические диски не найдены.");
    }
    else {
        for (size_t i = 0; i < m_Disks.size(); ++i) {
            const bool selected = m_Context.SelectedDisk && m_Context.SelectedDisk->Number == m_Disks[i].Number;
            if (DiskCard::Draw(m_Disks[i], selected)) {
                m_Context.SelectedDisk = m_Disks[i];
            }
        }
    }

    const bool diskSelected = m_Context.SelectedDisk.has_value();

    Layout::Space(30);

    ImGui::Separator();
    Layout::Space(15);

    constexpr float buttonWidth = 180.0f;
    constexpr float buttonHeight = 45.0f;

    // Назад
    if (ImGui::Button("Назад", ImVec2(buttonWidth, buttonHeight))) {
        m_BackRequested = true;
    }

    // Далее справа
    float right = ImGui::GetContentRegionAvail().x - buttonWidth;

    if (right > 0.0f)
        ImGui::SameLine(right + ImGui::GetCursorPosX());

    if (!diskSelected)
        ImGui::BeginDisabled();

    if (ImGui::Button("Далее", ImVec2(buttonWidth, buttonHeight))) {
        m_NextRequested = true;
    }

    if (!diskSelected)
        ImGui::EndDisabled();

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool InstallPage::BackRequested() const {
    return m_BackRequested;
}

bool InstallPage::NextRequested() const {
    return m_NextRequested;
}

void InstallPage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;
}