#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "DiskInfo.h"
#include "Packages/PackageManager.h"

enum class InstallMode {
    ReinstallWindows,
    ReinstallWindowsAndFormatData,
    CleanDisk
};

class InstallerContext {
  public:
    // Выбранный диск
    std::optional<DiskInfo> SelectedDisk;
    // Способ установки
    InstallMode InstallMode = InstallMode::ReinstallWindows;
    // Используется только при полной очистке диска
    std::uint64_t WindowsPartitionSize = 150ull * 1024 * 1024 * 1024;
    // Выбранная редакция Windows
    int SelectedEdition = 2;
    // Устанавливать драйверы
    bool InstallDrivers = true;
    // Выбранные программы
    std::vector<std::string> SelectedPackages;
    // Каталог доступных программ
    PackageManager Packages;
};