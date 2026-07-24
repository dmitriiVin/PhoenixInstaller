#pragma once

#include <optional>

#include "DiskInfo.h"

class InstallerContext {
  public:
    std::optional<DiskInfo> SelectedDisk;

    int SelectedEdition = -1;

    bool InstallDrivers = true;
    bool InstallPrograms = true;
};