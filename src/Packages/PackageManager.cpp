#include "PackageManager.h"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool PackageManager::Load(const std::string &file) {
    Clear();

    std::ifstream stream(file);

    if (!stream.is_open())
        return false;

    json root;

    try {
        stream >> root;
    }
    catch (...) {
        return false;
    }

    if (!root.is_array())
        return false;

    m_packages.reserve(root.size());

    for (const auto &item : root) {
        if (!item.is_object())
            continue;

        Package package;

        package.Id = item.value("id", "");
        package.Name = item.value("name", "");
        package.Folder = item.value("folder", "");
        package.Installer = item.value("installer", "");
        package.Arguments = item.value("arguments", "");

        if (package.Id.empty())
            continue;

        m_packages.emplace_back(std::move(package));
    }

    return true;
}

const Package *PackageManager::FindById(const std::string &id) const {
    for (const auto &package : m_packages) {
        if (package.Id == id)
            return &package;
    }

    return nullptr;
}

const std::vector<Package> &PackageManager::GetPackages() const {
    return m_packages;
}

void PackageManager::Clear() {
    m_packages.clear();
}