#include "DiskCard.h"

#include "Card.h"
#include "UI/Fonts/IconsFontAwesome6.h"

#include <cstdio>
#include <imgui.h>

namespace {

std::string FormatSize(uint64_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    char buffer[32];

    if (bytes >= static_cast<uint64_t>(TB))
        std::snprintf(buffer, sizeof(buffer), "%.2f TB", bytes / TB);
    else if (bytes >= static_cast<uint64_t>(GB))
        std::snprintf(buffer, sizeof(buffer), "%.1f GB", bytes / GB);
    else if (bytes >= static_cast<uint64_t>(MB))
        std::snprintf(buffer, sizeof(buffer), "%.0f MB", bytes / MB);
    else if (bytes >= static_cast<uint64_t>(KB))
        std::snprintf(buffer, sizeof(buffer), "%.0f KB", bytes / KB);
    else
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));

    return buffer;
}

const char *GetDiskType(const DiskInfo &disk) {
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

const char *GetPartitionType(const PartitionInfo &part) {
    switch (part.Role) {
    case PartitionRole::EFI:
        return "EFI";

    case PartitionRole::MSR:
        return "MSR";

    case PartitionRole::Windows:
        return "Windows";

    case PartitionRole::Recovery:
        return "Recovery";

    case PartitionRole::Data:
        return "Data";

    default:
        return "Unknown";
    }
}

} // namespace

bool DiskCard::Draw(const DiskInfo &disk, bool selected) {
    ImGui::PushID(disk.Number);

    Card::Begin("DiskCard", selected);

    // Заголовок
    ImGui::Text("%s", ICON_FA_HARD_DRIVE);
    ImGui::SameLine();
    ImGui::Text("%ls", disk.Model.c_str());

    ImGui::Separator();

    // Информация о диске
    ImGui::Text("Disk %u  |  %s  |  %s  |  %s", disk.Number, GetDiskType(disk), disk.IsGPT ? "GPT" : "MBR", FormatSize(disk.Size).c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (disk.Partitions.empty()) {
        ImGui::TextDisabled("No partitions");
    }
    else {
        for (const auto &part : disk.Partitions) {
            std::string title = GetPartitionType(part);

            if (part.Letter) {
                title += " (";
                title += static_cast<char>(part.Letter);
                title += ":)";
            }

            std::string size = FormatSize(part.Size);

            ImGui::Bullet();
            ImGui::SameLine();

            ImGui::TextUnformatted(title.c_str());

            if (!part.Label.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("[%ls]", part.Label.c_str());
            }

            ImVec2 textSize = ImGui::CalcTextSize(size.c_str());

            float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

            ImGui::SameLine(right - ImGui::GetCursorScreenPos().x - textSize.x);

            ImGui::TextDisabled("%s", size.c_str());
        }
    }

    bool clicked = Card::End();

    ImGui::PopID();

    return clicked;
}