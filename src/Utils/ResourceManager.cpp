#include "ResourceManager.h"

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