#pragma once

#include <string>

struct Package {
    std::string Id;
    std::string Name;

    std::string Folder;
    std::string Installer;
    std::string Arguments;
};