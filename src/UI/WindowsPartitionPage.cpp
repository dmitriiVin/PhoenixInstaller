#include "WindowsPartitionPage.h"

#include "UI/Fonts/Fonts.h"
#include "UI/Fonts/IconsFontAwesome6.h"
#include "UI/Fonts/IconsFontAwesome6Brands.h"
#include "UI/Layout/Layout.h"
#include "UI/Widgets/SegmentedSlider.h"
#include "UI/Widgets/ValueStepper.h"
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace {
constexpr uint64_t GB = 1024ull * 1024ull * 1024ull;
constexpr int MinWindowsSizeGB = 100;
} // namespace

WindowsPartitionPage::WindowsPartitionPage(InstallerContext &context) : m_Context(context) {
    m_WindowsSizeGB = static_cast<int>(m_Context.WindowsPartitionSize / GB);
}

void WindowsPartitionPage::Draw() {
    if (!m_Context.SelectedDisk) {
        ImGui::TextUnformatted("Диск не выбран.");
        return;
    }

    const int diskSizeGB = static_cast<int>(m_Context.SelectedDisk->Size / GB);

    const int maxWindowsSizeGB = std::max(MinWindowsSizeGB, diskSizeGB - 1);

    const int dataSizeGB = std::max(0, diskSizeGB - m_WindowsSizeGB);

    char windowsPartition[32];
    std::snprintf(windowsPartition, sizeof(windowsPartition), "%d ГБ", m_WindowsSizeGB);

    char dataPartition[32];
    std::snprintf(dataPartition, sizeof(dataPartition), "%d ГБ", dataSizeGB);

    Layout::Begin();
    Layout::BeginContent();

    const float width = std::min(Layout::Width() - Layout::Margin() * 2.0f, Layout::Scale(900.0f));

    Layout::BeginContainer(width);

    //------------------------------------------------------------

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Размер раздела Windows");
    ImGui::PopFont();

    Layout::Space(8);

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width * 0.55f);

    ImGui::TextWrapped("Укажите размер раздела Windows.\n"
                       "Оставшееся место будет автоматически "
                       "выделено под раздел данных.");

    ImGui::PopTextWrapPos();

    Layout::Space(22);

    //------------------------------------------------------------

    ImGui::PushFont(Fonts::Heading());
    ImGui::TextUnformatted("Размер раздела Windows");
    ImGui::PopFont();

    Layout::Space(10);

    if (SegmentedSlider::Draw("##PartitionSlider", &m_WindowsSizeGB, MinWindowsSizeGB, maxWindowsSizeGB)) {
        m_Context.WindowsPartitionSize = static_cast<uint64_t>(m_WindowsSizeGB) * GB;
    }

    Layout::Space(10);

    ImGui::Text("Минимум: %d ГБ", MinWindowsSizeGB);

    char maxBuffer[32];
    std::snprintf(maxBuffer, sizeof(maxBuffer), "Максимум: %d ГБ", maxWindowsSizeGB);

    float maxWidth = ImGui::CalcTextSize(maxBuffer).x;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - maxWidth);

    ImGui::TextUnformatted(maxBuffer);

    Layout::Space(18);

    constexpr float StepperWidth = 240.0f;

    // Центрируем виджет
    float x = (ImGui::GetContentRegionAvail().x - StepperWidth) * 0.5f;

    if (x > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);

    if (ValueStepper::Draw("##WindowsSize", &m_WindowsSizeGB, MinWindowsSizeGB, maxWindowsSizeGB, StepperWidth, 42.0f)) {
        m_Context.WindowsPartitionSize = static_cast<uint64_t>(m_WindowsSizeGB) * GB;
    }

    Layout::Space(16);

    ImGui::PushTextWrapPos();

    ImGui::TextWrapped(ICON_FA_CIRCLE_INFO " Минимальный размер — 100 ГБ.\n"
                                           "Рекомендуемый размер — не менее 150 ГБ.");

    ImGui::PopTextWrapPos();

    Layout::Space(24);

    //------------------------------------------------------------

    ImGui::PushFont(Fonts::Heading());
    ImGui::TextUnformatted("После установки будет создано");
    ImGui::PopFont();

    Layout::Space(10);

    ImGui::Separator();

    Layout::Space(10);

    ImGui::TextUnformatted(ICON_FA_WINDOWS " Windows (C:)");

    float windowsWidth = ImGui::CalcTextSize(windowsPartition).x;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - windowsWidth);

    ImGui::TextUnformatted(windowsPartition);

    Layout::Space(8);

    ImGui::Separator();

    Layout::Space(8);

    ImGui::TextUnformatted(ICON_FA_HARD_DRIVE " Data (D:)");

    float dataWidth = ImGui::CalcTextSize(dataPartition).x;

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - dataWidth);

    ImGui::TextUnformatted(dataPartition);

    Layout::Space(18);

    ImGui::TextDisabled(ICON_FA_CIRCLE_INFO " EFI, MSR и Recovery будут созданы автоматически.");

    Layout::Space(24);

    ImGui::Separator();

    Layout::Space(18);

    constexpr float ButtonWidth = 180.0f;
    constexpr float ButtonHeight = 44.0f;

    if (ImGui::Button("Назад", ImVec2(ButtonWidth, ButtonHeight))) {
        m_BackRequested = true;
    }

    float right = ImGui::GetContentRegionAvail().x - ButtonWidth;

    if (right > 0.0f)
        ImGui::SameLine(right + ImGui::GetCursorPosX());

    if (ImGui::Button("Далее", ImVec2(ButtonWidth, ButtonHeight))) {
        m_NextRequested = true;
    }

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool WindowsPartitionPage::BackRequested() const {
    return m_BackRequested;
}

bool WindowsPartitionPage::NextRequested() const {
    return m_NextRequested;
}

void WindowsPartitionPage::ResetState() {
    m_BackRequested = false;
    m_NextRequested = false;

    m_WindowsSizeGB = static_cast<int>(m_Context.WindowsPartitionSize / GB);
}