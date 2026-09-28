#pragma once

#include "Core/InstallerContext.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class LinuxInstallationLauncher {
  public:
    using ProgressCallback = std::function<void(float progress, const std::string &status)>;

    static bool Install(const InstallerContext &context, std::string &error, ProgressCallback progress = {});

  private:
    struct ExistingPartitions {
        std::string efi;
        std::string windows;
        std::string data;
    };

    struct BlockPartition {
        std::string device;
        std::string type;
        std::string filesystem;
        std::string label;
        std::string partType;
    };

    static bool IsUEFI();

    static bool RunCommand(const std::string &program, const std::vector<std::string> &arguments, std::string &error);

    static bool RunPrivilegedCommand(const std::string &program, const std::vector<std::string> &arguments, std::string &error);

    static bool RunPrivilegedCommandWithInput(const std::string &program, const std::vector<std::string> &arguments, const std::string &input, std::string &error);

    static bool RunCommandCapture(const std::string &program, const std::vector<std::string> &arguments, std::string &output, std::string &error);

    static bool GetExecutableDirectory(std::filesystem::path &directory, std::string &error);

    static bool InstallUEFI(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool InstallLegacy(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool InstallLegacyCleanDisk(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool InstallExistingWindows(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool InstallExistingWindowsAndData(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool InstallCleanDisk(const InstallerContext &context, std::string &error, const ProgressCallback &progress);

    static bool FindExistingPartitions(const InstallerContext &context, ExistingPartitions &partitions, std::string &error);

    static bool ParseLsblkOutput(const std::string &output, std::vector<BlockPartition> &partitions, std::string &error);

    static std::string NormalizeDevice(const std::string &device);

    static bool CreatePostInstallConfig(const InstallerContext &context, const std::filesystem::path &configPath, std::string &error);
};