#include "ProgramPage.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"
#include "Widgets/ProgramCard.h"

#include <algorithm>

#include <imgui.h>

ProgramPage::ProgramPage(InstallerContext &context) : m_Context(context) {
}

void ProgramPage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Установка программ");
    ImGui::PopFont();

    Layout::Space(20);

    ImGui::TextUnformatted("Выберите программы, которые необходимо установить");

    Layout::Space(20);

    const auto &packages = m_Context.Packages.GetPackages();
    auto &selectedPackages = m_Context.SelectedPackages;

    if (packages.empty()) {
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Программы не найдены.");
    }
    else {
        for (const auto &package : packages) {
            const bool selected = std::find(selectedPackages.begin(), selectedPackages.end(), package.Id) != selectedPackages.end();

            if (ProgramCard::Draw(package, selected)) {
                if (selected) {
                    auto it = std::remove(selectedPackages.begin(), selectedPackages.end(), package.Id);

                    selectedPackages.erase(it, selectedPackages.end());
                }
                else {
                    selectedPackages.push_back(package.Id);
                }
            }

            ImGui::Spacing();
        }
    }

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

    if (ImGui::Button("Далее", ImVec2(buttonWidth, buttonHeight))) {
        m_NextRequested = true;
    }

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool ProgramPage::BackRequested() const {
    return m_BackRequested;
}

bool ProgramPage::NextRequested() const {
    return m_NextRequested;
}

void ProgramPage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;
}