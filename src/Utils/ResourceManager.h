#pragma once

#include <filesystem>
#include <vector>
#include <windows.h>

class ResourceManager
{
public:
    static std::vector<std::uint8_t> Load(int id);
    static bool Extract(int id, const std::filesystem::path &destination);
};
