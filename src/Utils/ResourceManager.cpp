#include "ResourceManager.h"

#include <fstream>

std::vector<std::uint8_t> ResourceManager::Load(int id)
{
    HRSRC res = FindResource(
        nullptr,
        MAKEINTRESOURCE(id),
        RT_RCDATA);

    if (!res)
        return {};

    DWORD size = SizeofResource(nullptr, res);

    HGLOBAL data = LoadResource(nullptr, res);

    void* ptr = LockResource(data);

    return std::vector<std::uint8_t>(
        (std::uint8_t*)ptr,
        (std::uint8_t*)ptr + size);
}

bool ResourceManager::Extract(int id, const std::filesystem::path &destination) {
    const auto data = Load(id);

    if (data.empty())
        return false;

    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);

    if (error)
        return false;

    std::ofstream file(destination, std::ios::binary | std::ios::trunc);

    if (!file.is_open())
        return false;

    file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));

    return file.good();
}
