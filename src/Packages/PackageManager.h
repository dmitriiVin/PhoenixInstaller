#pragma once

#include <string>
#include <vector>

#include "Package.h"

class PackageManager {
  public:
    bool Load(const std::string &file);

    const Package *FindById(const std::string &id) const;

    const std::vector<Package> &GetPackages() const;

    void Clear();

  private:
    std::vector<Package> m_packages;
};