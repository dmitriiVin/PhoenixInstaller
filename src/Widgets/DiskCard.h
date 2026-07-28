#pragma once

#include "Core/DiskManager.h"

class DiskCard {
  public:
    static bool Draw(const DiskInfo &disk, bool selected);
};