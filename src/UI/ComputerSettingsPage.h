#pragma once

#include "Core/InstallerContext.h"
#include "Page.h"

class ComputerSettingsPage : public Page {
  public:
    explicit ComputerSettingsPage(InstallerContext &context);

    void Draw() override;

    bool BackRequested() const;
    bool NextRequested() const;

    void ResetState() override;

  private:
    InstallerContext &m_Context;

    bool m_BackRequested = false;
    bool m_NextRequested = false;

    bool IsValidComputerName() const;
};