#pragma once

#include <filesystem>

class PhoenixSetup {
  public:
    static int Run(const std::filesystem::path &configPath);
};
