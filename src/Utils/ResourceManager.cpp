#include "ResourceManager.h"

#include <fstream>

std::vector<std::uint8_t> ResourceManager::Load(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
        return {};

    file.seekg(0, std::ios::end);

    const auto size = file.tellg();

    if (size <= 0)
        return {};

    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));

    file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));

    if (!file)
        return {};

    return data;
}

bool ResourceManager::Extract(const std::filesystem::path &source, const std::filesystem::path &destination) {
    const auto data = Load(source);

    if (data.empty())
        return false;

    std::error_code error;

    const auto parent = destination.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);

        if (error)
            return false;
    }

    std::ofstream file(destination, std::ios::binary | std::ios::trunc);

    if (!file.is_open())
        return false;

    file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));

    return file.good();
}