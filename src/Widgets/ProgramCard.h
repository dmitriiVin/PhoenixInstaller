#pragma once

#include "Packages/Package.h"

class ProgramCard {
  public:
    static bool Draw(const Package &package, bool selected);
};