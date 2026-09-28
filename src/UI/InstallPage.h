#pragma once

#include "Core/DiskInfo.h"
#include "Core/InstallerContext.h"
#include "Page.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class InstallPage : public Page {
  public:
    explicit InstallPage(InstallerContext &context);

    void Draw() override;

    bool BackRequested() const;
    bool NextRequested() const;

    void ResetState();

    void StartInstallation();

    void SetProgress(float progress, const std::string &status);

    void SetFinished();

    void SetFailed(const std::string &error);

    bool IsInstalling() const;
    bool IsFinished() const;
    bool IsFailed() const;

  private:
    void DrawInstallationProgress();

  private:
    InstallerContext &m_Context;

    std::vector<DiskInfo> m_Disks;

    bool m_BackRequested = false;
    bool m_NextRequested = false;

    std::atomic<float> m_Progress{0.0f};

    std::atomic<bool> m_Installing{false};
    std::atomic<bool> m_Finished{false};
    std::atomic<bool> m_Failed{false};

    mutable std::mutex m_StateMutex;

    std::string m_Status;
    std::string m_Error;
};