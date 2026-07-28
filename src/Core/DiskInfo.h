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

struct PartitionInfo {
    // Номер раздела на физическом диске
    uint32_t Number = 0;

    // Буква тома (C, D, E...), 0 если отсутствует
    wchar_t Letter = 0;

    // Метка тома ("Windows", "Data"...)
    std::wstring Label;

    // Размер раздела
    uint64_t Size = 0;

    // Свободное место
    uint64_t FreeSpace = 0;

    // Системные признаки
    bool IsEFI = false;
    bool IsRecovery = false;
    bool IsMSR = false;
};

struct DiskInfo {
    uint32_t Number = 0;

    std::wstring Model;

    uint64_t Size = 0;

    DiskBusType BusType = DiskBusType::Unknown;

    bool IsSSD = false;
    bool IsUSB = false;
    bool IsGPT = false;

    // Все разделы данного физического диска
    std::vector<PartitionInfo> Partitions;
};