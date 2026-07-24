#pragma once

#include <vector>

#include "DiskInfo.h"

class DiskManager {
  public:
    static std::vector<DiskInfo> Enumerate();
};