#include "ConfirmPage.h"

#include <string>

#include "Core/DiskInfo.h"
#include "Core/Utils.h"
#include "UI/Fonts/Fonts.h"
#include "UI/Layout/Layout.h"
#include "Widgets/Card.h"

#include "imgui.h"

ConfirmPage::ConfirmPage(InstallerContext &context) : m_Context(context) {
}

void ConfirmPage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(1100.0f);

    m_BackRequested = false;
    m_NextRequested = false;

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Подтверждение установки");
    ImGui::PopFont();

    Layout::Space(15);

    ImGui::TextUnformatted("Проверьте параметры перед началом установки Windows.");

    Layout::Space(25);

    constexpr float CardHeight = 600.0f;

    //----------------------------------------------------
    // Левая колонка
    //----------------------------------------------------

    float columnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) * 0.5f;

    ImGui::BeginChild("Left", ImVec2(columnWidth, CardHeight), true);

    ImGui::TextDisabled("Windows");
    ImGui::Separator();

    ImGui::Text("Редакция");
    ImGui::SameLine(180);
    ImGui::TextUnformatted("Windows 10 Enterprise");

    Layout::Space(15);

    ImGui::TextDisabled("Диск");
    ImGui::Separator();

    if (m_Context.SelectedDisk) {
        const auto &disk = *m_Context.SelectedDisk;

        auto model = WideToUtf8(disk.Model);
        ImGui::TextUnformatted(model.c_str());

        ImGui::Text("Модель");
        ImGui::SameLine(180);
        ImGui::TextUnformatted(model.c_str());

        ImGui::Text("Тип");
        ImGui::SameLine(180);

        if (disk.BusType == DiskBusType::NVMe)
            ImGui::TextUnformatted("NVMe SSD");
        else if (disk.BusType == DiskBusType::SATA)
            ImGui::TextUnformatted("SATA SSD");
        else
            ImGui::TextUnformatted("Диск");

        ImGui::Text("Таблица");
        ImGui::SameLine(180);
        ImGui::TextUnformatted(disk.IsGPT ? "GPT" : "MBR");

        ImGui::Text("Размер");
        ImGui::SameLine(180);
        ImGui::Text("%.1f ГБ", disk.Size / 1024.0 / 1024.0 / 1024.0);
    }

    Layout::Space(15);

    ImGui::TextDisabled("Будет создано");
    ImGui::Separator();

    switch (m_Context.InstallMode) {
    case InstallMode::ReinstallWindows:

        ImGui::Text("Windows (C:)");
        ImGui::SameLine(180);
        ImGui::TextUnformatted("Существующий раздел");

        ImGui::Text("Data (D:)");
        ImGui::SameLine(180);
        ImGui::TextUnformatted("Без изменений");

        break;

    case InstallMode::ReinstallWindowsAndFormatData:

        ImGui::Text("Windows (C:)");
        ImGui::SameLine(180);
        ImGui::TextUnformatted("Форматирование и переустановка");

        ImGui::Text("Data (D:)");
        ImGui::SameLine(180);
        ImGui::TextUnformatted("Форматирование");

        break;

    case InstallMode::CleanDisk: {
        constexpr double MB = 1024.0 * 1024.0;
        constexpr double GB = 1024.0 * 1024.0 * 1024.0;

        const auto &disk = *m_Context.SelectedDisk;

        double diskSize = disk.Size / GB;

        double windowsSize = m_Context.WindowsPartitionSize / GB;

        constexpr double efiSize = 0.1;      // 100 МБ
        constexpr double recoverySize = 0.8; // 800 МБ

        double dataSize = diskSize - windowsSize - efiSize - recoverySize;

        ImGui::Text("EFI");
        ImGui::SameLine(180);
        ImGui::Text("100 МБ");

        ImGui::Text("Windows (C:)");
        ImGui::SameLine(180);
        ImGui::Text("%.0f ГБ", windowsSize);

        ImGui::Text("Data (D:)");
        ImGui::SameLine(180);

        if (dataSize > 0)
            ImGui::Text("%.1f ГБ", dataSize);
        else
            ImGui::Text("-");
    } break;
    }

    Layout::Space(15);

    ImGui::TextDisabled("Пользователь");
    ImGui::Separator();

    ImGui::Text("Учетная запись");
    ImGui::SameLine(180);
    ImGui::TextUnformatted("systemsupport");

    ImGui::EndChild();

    ImGui::SameLine();

    //----------------------------------------------------
    // Правая колонка
    //----------------------------------------------------

    ImGui::BeginChild("Right", ImVec2(0, CardHeight), true);

    ImGui::TextDisabled("Программы");
    ImGui::Separator();

    if (m_Context.SelectedPackages.empty()) {
        ImGui::TextDisabled("Не выбраны");
    }
    else {
        const float half = ImGui::GetContentRegionAvail().x * 0.5f;

        for (size_t i = 0; i < m_Context.SelectedPackages.size(); ++i) {
            ImGui::Text("✓ %s", m_Context.SelectedPackages[i].c_str());

            if (i % 2 == 0 && i + 1 < m_Context.SelectedPackages.size()) {
                ImGui::SameLine(half);
            }
        }
    }

    ImGui::EndChild();

    Layout::Space(15);

    ImGui::TextColored(ImVec4(1.f, .82f, .2f, 1.f), "Внимание!");

    switch (m_Context.InstallMode) {
    case InstallMode::ReinstallWindows:
        ImGui::TextWrapped("Будет переустановлена Windows. "
                           "Раздел Data (D:) будет сохранён.");
        break;

    case InstallMode::ReinstallWindowsAndFormatData:
        ImGui::TextWrapped("Разделы Windows и Data будут отформатированы.");
        break;

    case InstallMode::CleanDisk:
        ImGui::TextWrapped("Все разделы выбранного диска будут удалены.");
        break;
    }

    Layout::Space(15);

    ImGui::Separator();

    Layout::Space(10);

    constexpr float ButtonWidth = 250.0f;
    constexpr float ButtonHeight = 45.0f;

    if (ImGui::Button("Назад", ImVec2(ButtonWidth, ButtonHeight)))
        m_BackRequested = true;

    float right = ImGui::GetContentRegionAvail().x - ButtonWidth;

    if (right > 0.0f)
        ImGui::SameLine(ImGui::GetCursorPosX() + right);

    if (ImGui::Button("Установить Windows", ImVec2(ButtonWidth, ButtonHeight)))
        m_NextRequested = true;

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool ConfirmPage::BackRequested() const {
    return m_BackRequested;
}

bool ConfirmPage::NextRequested() const {
    return m_NextRequested;
}

void ConfirmPage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;
}