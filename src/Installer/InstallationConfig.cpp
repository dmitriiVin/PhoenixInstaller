#include "InstallationConfig.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace {

const char *ToConfigMode(InstallMode mode) {
    switch (mode) {
    case InstallMode::ReinstallWindows:
        return "reinstall_windows";

    case InstallMode::ReinstallWindowsAndFormatData:
        return "reinstall_windows_and_format_data";

    case InstallMode::CleanDisk:
        return "clean_disk";
    }

    return "unknown";
}

const char *ToConfigBusType(DiskBusType type) {
    switch (type) {
    case DiskBusType::SATA:
        return "sata";

    case DiskBusType::NVMe:
        return "nvme";

    case DiskBusType::USB:
        return "usb";

    case DiskBusType::SAS:
        return "sas";

    case DiskBusType::Unknown:
        return "unknown";
    }

    return "unknown";
}

const PartitionInfo *FindSinglePartition(const DiskInfo &disk, PartitionRole role) {
    const PartitionInfo *result = nullptr;

    for (const auto &partition : disk.Partitions) {
        if (partition.Role != role)
            continue;

        if (result)
            return nullptr;

        result = &partition;
    }

    return result;
}

} // namespace

bool InstallationConfig::Write(const InstallerContext &context, const std::filesystem::path &imagePath, const std::filesystem::path &destination, std::string &error) {
    if (!context.SelectedDisk) {
        error = "A target disk must be selected.";
        return false;
    }

    if (context.SelectedEdition <= 0) {
        error = "A Windows image edition must be selected.";
        return false;
    }

    const auto &disk = *context.SelectedDisk;

    nlohmann::json root;

    root["disk"] = {{"number", disk.Number}, {"device", disk.Device}, {"model", disk.Model}, {"size_bytes", disk.Size}, {"bus_type", ToConfigBusType(disk.BusType)},
                    {"ssd", disk.IsSSD},     {"usb", disk.IsUSB},     {"gpt", disk.IsGPT}};

    root["installation"] = {{"mode", ToConfigMode(context.Mode)}, {"windows_partition_size_gb", context.WindowsPartitionSize / (1024ull * 1024ull * 1024ull)}};

    if (context.Mode != InstallMode::CleanDisk) {
        const PartitionInfo *windowsPartition = FindSinglePartition(disk, PartitionRole::Windows);

        if (!windowsPartition) {
            error = "The selected disk does not contain one "
                    "unambiguous Windows partition.";

            return false;
        }

        root["installation"]["partitions"]["windows_number"] = windowsPartition->Number;

        if (context.Mode == InstallMode::ReinstallWindowsAndFormatData) {
            const PartitionInfo *dataPartition = FindSinglePartition(disk, PartitionRole::Data);

            if (!dataPartition) {
                error = "The selected disk does not contain one "
                        "unambiguous Data partition.";

                return false;
            }

            if (dataPartition->Number == windowsPartition->Number) {
                error = "Windows and Data partitions cannot be "
                        "the same partition.";

                return false;
            }

            root["installation"]["partitions"]["data_number"] = dataPartition->Number;
        }
    }

    root["windows"] = {{"image", imagePath.string()}, {"edition", context.SelectedEdition}};

    root["drivers"] = {{"install", context.InstallDrivers}};

    root["packages"] = context.SelectedPackages;

    std::error_code filesystemError;

    const auto parent = destination.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystemError);

        if (filesystemError) {
            error = "Unable to create the configuration directory.";

            return false;
        }
    }

    std::ofstream file(destination, std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        error = "Unable to create config.json.";
        return false;
    }

    file << root.dump(4) << '\n';

    if (!file.good()) {
        error = "Unable to write config.json.";
        return false;
    }

    return true;
}