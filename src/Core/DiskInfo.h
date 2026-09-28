#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class DiskBusType {
    Unknown,
    SATA,
    NVMe,
    USB,
    SAS
};

enum class PartitionRole {
    Unknown,
    EFI,
    MSR,
    Windows,
    Recovery,
    Data
};

struct PartitionInfo {
    // Номер раздела на физическом диске
    std::uint32_t Number = 0;

    // Точка монтирования, пустая если раздел не смонтирован
    std::string MountPoint;

    // Метка тома
    std::string Label;

    // Файловая система: ntfs, vfat, ext4 и т.д.
    std::string Filesystem;

    // Размер раздела
    std::uint64_t Size = 0;

    // Свободное место
    std::uint64_t FreeSpace = 0;

    // Назначение раздела
    PartitionRole Role = PartitionRole::Unknown;
};

struct DiskInfo {
    std::uint32_t Number = 0;

    std::string Device;
    std::string Model;

    std::uint64_t Size = 0;

    DiskBusType BusType = DiskBusType::Unknown;

    bool IsSSD = false;
    bool IsUSB = false;
    bool IsGPT = false;

    std::vector<PartitionInfo> Partitions;
};