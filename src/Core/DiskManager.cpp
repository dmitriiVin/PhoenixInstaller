#include "DiskManager.h"

#include <windows.h>
#include <winioctl.h>

#include <string>
#include <vector>


namespace {
std::wstring AnsiToWide(const char *str) {
    if (!str || !*str)
        return L"";

    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);

    if (len <= 0)
        return L"";

    std::wstring result(len - 1, L'\0');

    MultiByteToWideChar(CP_ACP, 0, str, -1, result.data(), len);

    return result;
}
} // namespace

std::vector<DiskInfo> DiskManager::Enumerate() {
    std::vector<DiskInfo> disks;

    for (DWORD index = 0; index < 32; index++) {
        std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(index);

        HANDLE hDisk = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (hDisk == INVALID_HANDLE_VALUE)
            continue;

        DiskInfo info{};
        info.Number = index;

        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        BYTE buffer[4096]{};

        DWORD returned = 0;

        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer, sizeof(buffer), &returned, nullptr)) {
            auto *descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR *>(buffer);

            std::wstring vendor;
            std::wstring product;

            if (descriptor->VendorIdOffset) {
                vendor = AnsiToWide(reinterpret_cast<char *>(buffer) + descriptor->VendorIdOffset);
            }

            if (descriptor->ProductIdOffset) {
                product = AnsiToWide(reinterpret_cast<char *>(buffer) + descriptor->ProductIdOffset);
            }

            info.Model = vendor;

            if (!info.Model.empty() && !product.empty())
                info.Model += L" ";

            info.Model += product;

            switch (descriptor->BusType) {
            case BusTypeUsb:
                info.BusType = DiskBusType::USB;
                info.IsUSB = true;
                break;

            case BusTypeNvme:
                info.BusType = DiskBusType::NVMe;
                info.IsSSD = true;
                break;

            case BusTypeAta:
            case BusTypeSata:
                info.BusType = DiskBusType::SATA;
                break;

            case BusTypeSas:
                info.BusType = DiskBusType::SAS;
                break;

            default:
                info.BusType = DiskBusType::Unknown;
                break;
            }
        }

        GET_LENGTH_INFORMATION lengthInfo{};

        if (DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &returned, nullptr)) {
            info.Size = static_cast<uint64_t>(lengthInfo.Length.QuadPart);
        }

        DRIVE_LAYOUT_INFORMATION_EX layout{};

        if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, &layout, sizeof(layout), &returned, nullptr)) {
            info.IsGPT = layout.PartitionStyle == PARTITION_STYLE_GPT;
        }

        CloseHandle(hDisk);

        disks.push_back(std::move(info));
    }

    return disks;
}