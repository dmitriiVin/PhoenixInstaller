#pragma once

#include <vector>
#include <windows.h>

class ResourceManager
{
public:
    static std::vector<std::uint8_t> Load(int id);
};