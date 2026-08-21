#pragma once

#include <filesystem>
#include <string>

#include "Core/InstallerContext.h"

class InstallationConfig {
  public:
    static bool Write(const InstallerContext &context, const std::filesystem::path &imagePath, const std::filesystem::path &destination, std::string &error);
};
