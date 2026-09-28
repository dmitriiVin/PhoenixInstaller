#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ResourceManager {
  public:
    static std::vector<std::uint8_t> Load(const std::filesystem::path &path);

    static bool Extract(const std::filesystem::path &source, const std::filesystem::path &destination);
};