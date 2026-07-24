#pragma once

#include "Core/DiskInfo.h"
#include "Core/InstallerContext.h"
#include "Page.h"

#include <vector>

class InstallPage : public Page {
  public:
    explicit InstallPage(InstallerContext &context);

    void Draw() override;

    bool NextRequested() const;
    void ResetState();

  private:
    InstallerContext &m_Context;

    std::vector<DiskInfo> m_Disks;

    bool m_NextRequested = false;
};