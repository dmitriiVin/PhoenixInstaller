#pragma once

#include <cstdint>
#include <string>

enum class DiskBusType {
    Unknown,
    SATA,
    NVMe,
    USB,
    SAS
};

struct DiskInfo {
    uint32_t Number = 0;

    std::wstring Model;

    uint64_t Size = 0;

    DiskBusType BusType = DiskBusType::Unknown;

    bool IsSSD = false;
    bool IsUSB = false;
    bool IsGPT = false;
};