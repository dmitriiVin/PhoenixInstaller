#include "DiskManager.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <winioctl.h>

namespace {

const GUID kPartitionSystemGuid = {0xC12A7328, 0xF81F, 0x11D2, {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}};

const GUID kPartitionMsrGuid = {0xE3C9E316, 0x0B5C, 0x4DB8, {0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE}};

const GUID kPartitionRecoveryGuid = {0xDE94BBA4, 0x06D1, 0x4D40, {0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC}};

DiskBusType ConvertBusType(STORAGE_BUS_TYPE type) {
    switch (type) {
    case BusTypeAta:
    case BusTypeSata:
        return DiskBusType::SATA;

    case BusTypeNvme:
        return DiskBusType::NVMe;

    case BusTypeUsb:
        return DiskBusType::USB;

    case BusTypeScsi:
    case BusTypeSas:
        return DiskBusType::SAS;

    default:
        return DiskBusType::Unknown;
    }
}

bool QueryStorageDescriptor(HANDLE disk, DiskInfo &info) {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE buffer[1024]{};
    DWORD returned = 0;

    if (!DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer, sizeof(buffer), &returned, nullptr)) {
        return false;
    }

    auto *descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR *>(buffer);

    info.BusType = ConvertBusType(descriptor->BusType);
    info.IsUSB = descriptor->BusType == BusTypeUsb;

    STORAGE_PROPERTY_QUERY seekQuery{};
    seekQuery.PropertyId = StorageDeviceSeekPenaltyProperty;
    seekQuery.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenalty{};
    returned = 0;

    if (DeviceIoControl(disk, IOCTL_STORAGE_QUERY_PROPERTY, &seekQuery, sizeof(seekQuery), &seekPenalty, sizeof(seekPenalty), &returned, nullptr)) {
        info.IsSSD = !seekPenalty.IncursSeekPenalty;
    }
    else {
        info.IsSSD = descriptor->BusType == BusTypeNvme;
    }

    if (descriptor->ProductIdOffset) {
        auto *model = reinterpret_cast<const char *>(buffer) + descriptor->ProductIdOffset;

        int len = MultiByteToWideChar(CP_ACP, 0, model, -1, nullptr, 0);

        if (len > 1) {
            info.Model.resize(len - 1);

            MultiByteToWideChar(CP_ACP, 0, model, -1, info.Model.data(), len);
        }
    }

    return true;
}

bool QueryDiskSize(HANDLE disk, DiskInfo &info) {
    GET_LENGTH_INFORMATION length{};
    DWORD returned = 0;

    if (!DeviceIoControl(disk, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length), &returned, nullptr)) {
        return false;
    }

    info.Size = length.Length.QuadPart;

    return true;
}

bool QueryPartitionStyle(HANDLE disk, DiskInfo &info) {
    BYTE buffer[4096]{};
    DWORD returned = 0;

    if (!DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, buffer, sizeof(buffer), &returned, nullptr)) {
        return false;
    }

    auto *layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX *>(buffer);

    info.IsGPT = layout->PartitionStyle == PARTITION_STYLE_GPT;

    return true;
}

} // namespace

std::vector<DiskInfo> DiskManager::Enumerate() {
    std::vector<DiskInfo> disks;

    for (DWORD number = 0;; ++number) {
        std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(number);

        HANDLE disk = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (disk == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
                break;

            continue;
        }

        DiskInfo info{};
        info.Number = number;

        QueryStorageDescriptor(disk, info);
        QueryDiskSize(disk, info);
        QueryPartitionStyle(disk, info);

        CloseHandle(disk);

        EnumeratePartitions(info);

        disks.push_back(std::move(info));
    }

    return disks;
}
namespace {
bool QueryDriveLayout(DWORD diskNumber, DRIVE_LAYOUT_INFORMATION_EX *&layout) {
    layout = nullptr;

    std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);

    HANDLE disk = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (disk == INVALID_HANDLE_VALUE)
        return false;

    DWORD size = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * 128;

    auto *buffer = new BYTE[size]{};

    DWORD returned = 0;

    if (!DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, buffer, size, &returned, nullptr)) {
        delete[] buffer;
        CloseHandle(disk);
        return false;
    }

    CloseHandle(disk);

    layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX *>(buffer);

    return true;
}
} // namespace

void DiskManager::EnumeratePartitions(DiskInfo &disk) {
    DRIVE_LAYOUT_INFORMATION_EX *layout = nullptr;

    if (!QueryDriveLayout(disk.Number, layout))
        return;

    wchar_t volumeName[MAX_PATH]{};

    HANDLE find = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));

    if (find == INVALID_HANDLE_VALUE) {
        delete[] reinterpret_cast<BYTE *>(layout);
        return;
    }

    do {
        std::wstring volumePath = volumeName;
        volumePath.pop_back(); // убрать завершающий '\'

        HANDLE volume = CreateFileW(volumePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (volume == INVALID_HANDLE_VALUE)
            continue;

        BYTE extentBuffer[sizeof(VOLUME_DISK_EXTENTS) + sizeof(DISK_EXTENT) * 32]{};

        auto *extents = reinterpret_cast<VOLUME_DISK_EXTENTS *>(extentBuffer);

        DWORD returned = 0;

        if (!DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, extents, sizeof(extentBuffer), &returned, nullptr)) {
            CloseHandle(volume);
            continue;
        }

        bool belongsToDisk = false;

        LARGE_INTEGER startOffset{};
        startOffset.QuadPart = -1;

        for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i) {
            if (extents->Extents[i].DiskNumber != disk.Number)
                continue;

            belongsToDisk = true;
            startOffset = extents->Extents[i].StartingOffset;
            break;
        }

        if (!belongsToDisk) {
            CloseHandle(volume);
            continue;
        }

        PartitionInfo part{};

        for (DWORD i = 0; i < layout->PartitionCount; ++i) {
            const auto &p = layout->PartitionEntry[i];

            if (p.StartingOffset.QuadPart != startOffset.QuadPart)
                continue;

            part.Number = p.PartitionNumber;
            part.Size = p.PartitionLength.QuadPart;

            if (layout->PartitionStyle == PARTITION_STYLE_GPT) {
                const GUID &type = p.Gpt.PartitionType;

                if (IsEqualGUID(type, kPartitionSystemGuid))
                    part.Role = PartitionRole::EFI;
                else if (IsEqualGUID(type, kPartitionMsrGuid))
                    part.Role = PartitionRole::MSR;
                else if (IsEqualGUID(type, kPartitionRecoveryGuid))
                    part.Role = PartitionRole::Recovery;
            }

            break;
        }

        wchar_t paths[1024]{};

        DWORD len = 0;

        if (GetVolumePathNamesForVolumeNameW(volumeName, paths, ARRAYSIZE(paths), &len)) {
            if (paths[0] != 0)
                part.Letter = paths[0];
        }

        wchar_t label[MAX_PATH]{};

        if (GetVolumeInformationW(volumeName, label, ARRAYSIZE(label), nullptr, nullptr, nullptr, nullptr, 0)) {
            part.Label = label;
        }

        ULARGE_INTEGER total{};
        ULARGE_INTEGER free{};

        if (paths[0] != 0) {
            if (GetDiskFreeSpaceExW(paths, nullptr, &total, &free)) {
                part.Size = total.QuadPart;
                part.FreeSpace = free.QuadPart;
            }
        }

        disk.Partitions.push_back(std::move(part));

        CloseHandle(volume);

    } while (FindNextVolumeW(find, volumeName, ARRAYSIZE(volumeName)));

    FindVolumeClose(find);

    delete[] reinterpret_cast<BYTE *>(layout);

    std::sort(disk.Partitions.begin(), disk.Partitions.end(), [](const PartitionInfo &a, const PartitionInfo &b) { return a.Number < b.Number; });
}