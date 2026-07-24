#pragma once

#include "Core/DiskManager.h"

class DiskCard {
  public:
    static bool Draw(const DiskInfo &disk, bool selected);

  private:
    static const char *GetDiskType(const DiskInfo &);
    static double ToGB(uint64_t bytes);
};