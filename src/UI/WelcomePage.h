#pragma once

#include "Page.h"

class WelcomePage : public Page {
  public:
    void Draw() override;

    bool NextRequested() const;
    void ResetState();

  private:
    bool m_NextRequested = false;
};