#include "InstallPage.h"

#include "Core/DiskManager.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"
#include "Widgets/DiskCard.h"

#include <imgui.h>

#include <algorithm>

InstallPage::InstallPage(InstallerContext &context) : m_Context(context) {
    m_Disks = DiskManager::Enumerate();
}

void InstallPage::Draw() {
    if (IsInstalling() || IsFinished() || IsFailed()) {
        DrawInstallationProgress();
        return;
    }

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

    if (ImGui::Button("Назад", ImVec2(buttonWidth, buttonHeight))) {
        m_BackRequested = true;
    }

    float right = ImGui::GetContentRegionAvail().x - buttonWidth;

    if (right > 0.0f) {
        ImGui::SameLine(right + ImGui::GetCursorPosX());
    }

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

void InstallPage::DrawInstallationProgress() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    ImGui::PushFont(Fonts::Title());

    if (IsFinished()) {
        ImGui::TextUnformatted("Установка завершена");
    }
    else if (IsFailed()) {
        ImGui::TextUnformatted("Ошибка установки");
    }
    else {
        ImGui::TextUnformatted("Установка Windows");
    }

    ImGui::PopFont();

    Layout::Space(35);

    const float progress = std::clamp(m_Progress.load(), 0.0f, 1.0f);

    ImGui::ProgressBar(progress, ImVec2(-1.0f, 32.0f));

    Layout::Space(15);

    std::string status;
    std::string error;

    {
        std::lock_guard<std::mutex> lock(m_StateMutex);

        status = m_Status;
        error = m_Error;
    }

    ImGui::TextWrapped("%s", status.c_str());

    Layout::Space(15);

    ImGui::Text("%.0f%%", progress * 100.0f);

    if (IsFailed()) {
        Layout::Space(25);

        ImGui::Separator();

        Layout::Space(15);

        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Ошибка:");

        Layout::Space(8);

        ImGui::TextWrapped("%s", error.c_str());
    }

    if (IsFinished()) {
        Layout::Space(25);

        ImGui::Separator();

        Layout::Space(20);

        ImGui::TextUnformatted("Windows установлена и загрузчик "
                               "настроен.");
    }

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

void InstallPage::StartInstallation() {
    {
        std::lock_guard<std::mutex> lock(m_StateMutex);

        m_Status = "Подготовка установки...";

        m_Error.clear();
    }

    m_Progress.store(0.0f);

    m_Installing.store(true);
    m_Finished.store(false);
    m_Failed.store(false);
}

void InstallPage::SetProgress(float progress, const std::string &status) {
    progress = std::clamp(progress, 0.0f, 1.0f);

    m_Progress.store(progress);

    std::lock_guard<std::mutex> lock(m_StateMutex);

    m_Status = status;
}

void InstallPage::SetFinished() {
    m_Progress.store(1.0f);

    {
        std::lock_guard<std::mutex> lock(m_StateMutex);

        m_Status = "Установка Windows завершена.";

        m_Error.clear();
    }

    m_Installing.store(false);
    m_Finished.store(true);
    m_Failed.store(false);
}

void InstallPage::SetFailed(const std::string &error) {
    {
        std::lock_guard<std::mutex> lock(m_StateMutex);

        m_Status = "Установка завершилась с ошибкой.";

        m_Error = error;
    }

    m_Installing.store(false);
    m_Finished.store(false);
    m_Failed.store(true);
}

bool InstallPage::IsInstalling() const {
    return m_Installing.load();
}

bool InstallPage::IsFinished() const {
    return m_Finished.load();
}

bool InstallPage::IsFailed() const {
    return m_Failed.load();
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

    m_Progress.store(0.0f);

    m_Installing.store(false);
    m_Finished.store(false);
    m_Failed.store(false);

    std::lock_guard<std::mutex> lock(m_StateMutex);

    m_Status.clear();
    m_Error.clear();
}