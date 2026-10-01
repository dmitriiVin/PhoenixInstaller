#include "ComputerSettingsPage.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"

#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <string>

ComputerSettingsPage::ComputerSettingsPage(InstallerContext &context)
    : m_Context(context) {
}

void ComputerSettingsPage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Настройка компьютера");
    ImGui::PopFont();

    Layout::Space(10);

    ImGui::TextWrapped(
        "Укажите параметры компьютера, которые будут применены после установки Windows."
    );

    Layout::Space(25);

    // ------------------------------------------------------------
    // Имя компьютера
    // ------------------------------------------------------------

    ImGui::Checkbox(
        "Изменить имя компьютера",
        &m_Context.ChangeComputerName
    );

    Layout::Space(10);

    ImGui::TextUnformatted("Имя компьютера");

    Layout::Space(5);

    ImGui::BeginDisabled(!m_Context.ChangeComputerName);

    char computerName[256];

    std::snprintf(
        computerName,
        sizeof(computerName),
        "%s",
        m_Context.ComputerName.c_str()
    );

    if (ImGui::InputText(
            "##ComputerName",
            computerName,
            sizeof(computerName))) {

        m_Context.ComputerName = computerName;
    }

    ImGui::EndDisabled();

    if (m_Context.ChangeComputerName &&
        !m_Context.ComputerName.empty() &&
        !IsValidComputerName()) {

        ImGui::TextColored(
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "Некорректное имя компьютера."
        );
    }

    Layout::Space(30);

    ImGui::Separator();

    Layout::Space(18);

    // ------------------------------------------------------------
    // Кнопки
    // ------------------------------------------------------------

    constexpr float ButtonWidth = 180.0f;
    constexpr float ButtonHeight = 44.0f;

    if (ImGui::Button(
            "Назад",
            ImVec2(ButtonWidth, ButtonHeight))) {

        m_BackRequested = true;
    }

    float right =
        ImGui::GetContentRegionAvail().x - ButtonWidth;

    if (right > 0.0f)
        ImGui::SameLine(
            right + ImGui::GetCursorPosX()
        );

    const bool settingsValid =
        !m_Context.ChangeComputerName ||
        IsValidComputerName();

    ImGui::BeginDisabled(!settingsValid);

    if (ImGui::Button(
            "Далее",
            ImVec2(ButtonWidth, ButtonHeight))) {

        m_NextRequested = true;
    }

    ImGui::EndDisabled();

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool ComputerSettingsPage::IsValidComputerName() const {
    const std::string &name = m_Context.ComputerName;

    if (name.empty() || name.size() > 15)
        return false;

    for (char character : name) {
        if (std::isalnum(
                static_cast<unsigned char>(character))) {

            continue;
        }

        if (character == '-')
            continue;

        return false;
    }

    return true;
}

bool ComputerSettingsPage::BackRequested() const {
    return m_BackRequested;
}

bool ComputerSettingsPage::NextRequested() const {
    return m_NextRequested;
}

void ComputerSettingsPage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;
}