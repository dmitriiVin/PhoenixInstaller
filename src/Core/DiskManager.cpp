#include "DiskManager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include <sys/statvfs.h>

namespace fs = std::filesystem;

namespace {

constexpr const char *EFI_GUID = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b";

constexpr const char *MSR_GUID = "e3c9e316-0b5c-4db8-817d-f92df00215ae";

constexpr const char *WINDOWS_RECOVERY_GUID = "de94bba4-06d1-4d40-a16a-bfd50179d6ac";

constexpr const char *MICROSOFT_BASIC_DATA_GUID = "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7";

std::string ReadFile(const fs::path &path) {
    std::ifstream file(path);

    if (!file.is_open())
        return {};

    std::string value;
    std::getline(file, value);

    return value;
}

std::string Trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }

    return value;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return value;
}

std::string RunCommand(const std::string &command) {
    FILE *pipe = popen(command.c_str(), "r");

    if (!pipe)
        return {};

    char buffer[512];
    std::string output;

    while (fgets(buffer, sizeof(buffer), pipe))
        output += buffer;

    pclose(pipe);

    return Trim(output);
}

bool IsPhysicalDisk(const fs::path &path) {
    const std::string name = path.filename().string();

    if (name.rfind("loop", 0) == 0)
        return false;

    if (name.rfind("ram", 0) == 0)
        return false;

    if (name.rfind("zram", 0) == 0)
        return false;

    if (name.rfind("sr", 0) == 0)
        return false;

    if (name.rfind("fd", 0) == 0)
        return false;

    return true;
}

DiskBusType DetectBusType(const std::string &device) {
    const fs::path sysBlock = fs::path("/sys/block") / device;

    if (device.rfind("nvme", 0) == 0)
        return DiskBusType::NVMe;

    std::error_code error;

    const fs::path devicePath = sysBlock / "device";

    if (fs::exists(devicePath, error)) {
        const fs::path resolved = fs::canonical(devicePath, error);

        if (!error) {
            const std::string path = resolved.string();

            if (path.find("/usb") != std::string::npos)
                return DiskBusType::USB;

            if (path.find("/sas") != std::string::npos)
                return DiskBusType::SAS;
        }
    }

    if (device.rfind("sd", 0) == 0)
        return DiskBusType::SATA;

    if (device.rfind("mmc", 0) == 0)
        return DiskBusType::SATA;

    return DiskBusType::Unknown;
}

bool DetectSSD(const std::string &device) {
    const fs::path rotational = fs::path("/sys/block") / device / "queue/rotational";

    return Trim(ReadFile(rotational)) == "0";
}

std::uint64_t ReadDiskSize(const std::string &device) {
    const fs::path sizePath = fs::path("/sys/block") / device / "size";

    const std::string value = Trim(ReadFile(sizePath));

    if (value.empty())
        return 0;

    try {
        return std::stoull(value) * 512ULL;
    }
    catch (...) {
        return 0;
    }
}

std::uint64_t ReadPartitionSize(const fs::path &partitionPath) {
    const std::string value = Trim(ReadFile(partitionPath / "size"));

    if (value.empty())
        return 0;

    try {
        return std::stoull(value) * 512ULL;
    }
    catch (...) {
        return 0;
    }
}

std::string ReadModel(const std::string &device) {
    const fs::path modelPath = fs::path("/sys/block") / device / "device/model";

    return Trim(ReadFile(modelPath));
}

bool IsGPT(const std::string &device) {
    const std::string command = "lsblk -dnro PTTYPE /dev/" + device + " 2>/dev/null";

    return RunCommand(command) == "gpt";
}

std::string ReadMountPoint(const std::string &device) {
    std::ifstream mounts("/proc/self/mounts");

    if (!mounts.is_open())
        return {};

    const std::string target = "/dev/" + device;

    std::string line;

    while (std::getline(mounts, line)) {
        std::istringstream stream(line);

        std::string source;
        std::string mountPoint;

        stream >> source >> mountPoint;

        if (source == target)
            return mountPoint;
    }

    return {};
}

std::uint64_t ReadFreeSpace(const std::string &mountPoint) {
    if (mountPoint.empty())
        return 0;

    struct statvfs info{};

    if (statvfs(mountPoint.c_str(), &info) != 0) {
        return 0;
    }

    return static_cast<std::uint64_t>(info.f_bavail) * static_cast<std::uint64_t>(info.f_frsize);
}

std::string ReadProperty(const std::string &device, const std::string &property) {
    const std::string command = "udevadm info --query=property "
                                "--name=/dev/" +
                                device + " 2>/dev/null";

    const std::string output = RunCommand(command);

    std::istringstream stream(output);

    std::string line;

    const std::string prefix = property + "=";

    while (std::getline(stream, line)) {
        if (line.rfind(prefix, 0) == 0)
            return line.substr(prefix.size());
    }

    return {};
}

std::string ReadPartitionLabel(const std::string &device) {
    return ReadProperty(device, "ID_FS_LABEL");
}

std::string ReadFilesystemType(const std::string &device) {
    return ReadProperty(device, "ID_FS_TYPE");
}

std::string ReadPartitionTypeGuid(const std::string &device) {
    return ToLower(ReadProperty(device, "ID_PART_ENTRY_TYPE"));
}

bool IsWindowsInstallation(const std::string &mountPoint) {
    if (mountPoint.empty())
        return false;

    const fs::path systemHive = fs::path(mountPoint) / "Windows/System32/Config/SYSTEM";

    return fs::exists(systemHive);
}

bool IsDataVolume(const std::string &label) {
    return ToLower(label) == "data";
}

PartitionRole DetectPartitionRole(const std::string &device, const std::string &mountPoint, const std::string &label, const std::string &filesystem, bool diskHasWindowsLayout) {
    /*
     * Если раздел смонтирован и на нём реально
     * присутствует Windows — это самый надёжный
     * способ определения.
     */
    if (IsWindowsInstallation(mountPoint))
        return PartitionRole::Windows;

    const std::string type = ReadPartitionTypeGuid(device);

    /*
     * EFI System Partition
     */
    if (type == EFI_GUID)
        return PartitionRole::EFI;

    /*
     * Microsoft Reserved Partition
     */
    if (type == MSR_GUID)
        return PartitionRole::MSR;

    /*
     * Windows Recovery Environment
     */
    if (type == WINDOWS_RECOVERY_GUID)
        return PartitionRole::Recovery;

    /*
     * Явно помеченный Data-раздел.
     */
    if (IsDataVolume(label))
        return PartitionRole::Data;

    /*
     * Windows Basic Data Partition.
     *
     * Важная часть:
     * NTFS + Microsoft Basic Data + наличие
     * EFI/MSR на этом же диске.
     *
     * Это позволяет определить Windows даже
     * когда раздел сейчас не смонтирован.
     */
    if (diskHasWindowsLayout && type == MICROSOFT_BASIC_DATA_GUID && ToLower(filesystem) == "ntfs") {
        return PartitionRole::Windows;
    }

    return PartitionRole::Unknown;
}

} // namespace

std::vector<DiskInfo> DiskManager::Enumerate() {
    std::vector<DiskInfo> disks;

    const fs::path blockPath("/sys/block");

    std::error_code error;

    if (!fs::exists(blockPath, error))
        return disks;

    for (const auto &entry : fs::directory_iterator(blockPath, error)) {
        if (error)
            break;

        if (!IsPhysicalDisk(entry.path()))
            continue;

        const std::string device = entry.path().filename().string();

        DiskInfo info{};

        info.Device = device;
        info.Model = ReadModel(device);
        info.Size = ReadDiskSize(device);
        info.BusType = DetectBusType(device);
        info.IsSSD = DetectSSD(device);
        info.IsUSB = info.BusType == DiskBusType::USB;
        info.IsGPT = IsGPT(device);

        EnumeratePartitions(info);

        disks.push_back(std::move(info));
    }

    std::sort(disks.begin(), disks.end(), [](const DiskInfo &a, const DiskInfo &b) { return a.Device < b.Device; });

    for (std::uint32_t i = 0; i < disks.size(); ++i) {
        disks[i].Number = i;
    }

    return disks;
}

void DiskManager::EnumeratePartitions(DiskInfo &disk) {
    const fs::path blockPath = fs::path("/sys/block") / disk.Device;

    std::error_code error;

    if (!fs::exists(blockPath, error))
        return;

    /*
     * Сначала определяем, есть ли на диске
     * характерная Windows-разметка.
     */
    bool hasEFI = false;
    bool hasMSR = false;

    for (const auto &entry : fs::directory_iterator(blockPath, error)) {
        if (error)
            break;

        if (!fs::exists(entry.path() / "partition", error)) {
            continue;
        }

        const std::string partition = entry.path().filename().string();

        const std::string type = ReadPartitionTypeGuid(partition);

        if (type == EFI_GUID)
            hasEFI = true;

        if (type == MSR_GUID)
            hasMSR = true;
    }

    const bool diskHasWindowsLayout = disk.IsGPT && hasEFI && hasMSR;

    /*
     * Теперь собираем сами разделы.
     */
    for (const auto &entry : fs::directory_iterator(blockPath, error)) {
        if (error)
            break;

        if (!fs::exists(entry.path() / "partition", error)) {
            continue;
        }

        const std::string partition = entry.path().filename().string();

        PartitionInfo info{};

        const std::string number = Trim(ReadFile(entry.path() / "partition"));

        try {
            info.Number = static_cast<std::uint32_t>(std::stoul(number));
        }
        catch (...) {
            continue;
        }

        info.Size = ReadPartitionSize(entry.path());

        info.MountPoint = ReadMountPoint(partition);

        info.Label = ReadPartitionLabel(partition);

        info.Filesystem = ReadFilesystemType(partition);

        if (!info.MountPoint.empty()) {
            info.FreeSpace = ReadFreeSpace(info.MountPoint);
        }

        info.Role = DetectPartitionRole(partition, info.MountPoint, info.Label, info.Filesystem, diskHasWindowsLayout);

        disk.Partitions.push_back(std::move(info));
    }

    std::sort(disk.Partitions.begin(), disk.Partitions.end(), [](const PartitionInfo &a, const PartitionInfo &b) { return a.Number < b.Number; });
}