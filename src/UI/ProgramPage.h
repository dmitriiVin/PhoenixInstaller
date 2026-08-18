#pragma once

#include "Core/InstallerContext.h"
#include "Page.h"

class ProgramPage : public Page {
  public:
    explicit ProgramPage(InstallerContext &context);

    void Draw() override;

    bool BackRequested() const;
    bool NextRequested() const;
    void ResetState();

  private:
    InstallerContext &m_Context;

    bool m_BackRequested = false;
    bool m_NextRequested = false;
};