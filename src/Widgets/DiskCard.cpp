#include "DiskCard.h"

#include "Card.h"
#include "UI/Fonts/IconsFontAwesome6.h"

#include <cstdio>
#include <imgui.h>

double DiskCard::ToGB(uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

const char *DiskCard::GetDiskType(const DiskInfo &disk) {
    switch (disk.BusType) {
    case DiskBusType::NVMe:
        return "NVMe SSD";

    case DiskBusType::SATA:
        return disk.IsSSD ? "SATA SSD" : "SATA HDD";

    case DiskBusType::USB:
        return "USB";

    case DiskBusType::SAS:
        return "SAS";

    default:
        return disk.IsSSD ? "SSD" : "HDD";
    }
}

bool DiskCard::Draw(const DiskInfo &disk, bool selected) {
    ImGui::PushID(disk.Number);

    bool clicked = Card::Begin("DiskCard", 150.0f, selected);

    ImGui::Text("%s  %ls", ICON_FA_HARD_DRIVE, disk.Model.c_str());

    ImGui::Separator();

    ImGui::Text("Disk %u", disk.Number);

    ImGui::SameLine();

    ImGui::TextDisabled("|");

    ImGui::SameLine();

    ImGui::Text("%s", GetDiskType(disk));

    ImGui::SameLine();

    ImGui::TextDisabled("|");

    ImGui::SameLine();

    ImGui::Text("%s", disk.IsGPT ? "GPT" : "MBR");

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f GB", ToGB(disk.Size));

    ImGui::Spacing();

    ImGui::TextDisabled("Место на диске: ");
    ImGui::SameLine();
    ImGui::Text("%s", buffer);

    Card::End();

    ImGui::PopID();

    return clicked;
}