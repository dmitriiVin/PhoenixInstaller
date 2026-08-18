#pragma once

#include <vector>

#include "DiskInfo.h"

class DiskManager {
  public:
    static std::vector<DiskInfo> Enumerate();

  private:
    static void EnumeratePartitions(DiskInfo &disk);
};