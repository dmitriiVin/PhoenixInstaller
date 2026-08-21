#pragma once

#include <string>

#include "Core/InstallerContext.h"

class InstallationLauncher {
  public:
    static bool Launch(const InstallerContext &context, std::wstring &error);
};
