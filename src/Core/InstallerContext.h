#pragma once

#include <optional>
#include <string>
#include <vector>

#include "DiskInfo.h"
#include "Packages/PackageManager.h"

class InstallerContext {
  public:
    std::optional<DiskInfo> SelectedDisk;

    int SelectedEdition = -1;

    bool InstallDrivers = true;

    std::vector<std::string> SelectedPackages;

    PackageManager Packages;
};