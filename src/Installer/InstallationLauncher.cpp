#include "InstallationLauncher.h"

#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Core/Utils.h"
#include "InstallationConfig.h"
#include "Utils/ResourceManager.h"
#include "resource.h"

namespace {

struct EmbeddedFile {
    int ResourceId;
    const wchar_t *RelativePath;
};

constexpr std::array<EmbeddedFile, 13> kPayloadFiles = {{
    {IDR_SCRIPT_INSTALL, L"Scripts\\Install.bat"},
    {IDR_SCRIPT_COMMON, L"Scripts\\Common.bat"},
    {IDR_SCRIPT_CLEAN_DISK, L"Scripts\\Disk\\CleanDisk.bat"},
    {IDR_SCRIPT_PREPARE_WINDOWS, L"Scripts\\Disk\\PrepareWindows.bat"},
    {IDR_SCRIPT_PREPARE_WINDOWS_AND_DATA, L"Scripts\\Disk\\PrepareWindowsAndData.bat"},
    {IDR_SCRIPT_APPLY_IMAGE, L"Scripts\\Windows\\ApplyImage.bat"},
    {IDR_SCRIPT_SETUP_BOOT, L"Scripts\\Windows\\SetupBoot.bat"},
    {IDR_SCRIPT_CONFIGURE_WINDOWS, L"Scripts\\Windows\\ConfigureWindows.bat"},
    {IDR_SCRIPT_INSTALL_DRIVERS, L"Scripts\\Drivers\\InstallDrivers.bat"},
    {IDR_SCRIPT_INSTALL_PROGRAMS, L"Scripts\\Programs\\InstallPrograms.bat"},
    {IDR_SCRIPT_WINDOWS_TWEAKS, L"Scripts\\Tweaks\\WindowsTweaks.bat"},
    {IDR_SCRIPT_SECURITY, L"Scripts\\Tweaks\\Security.ps1"},
    {IDR_PACKAGES_JSON, L"packages.json"},
}};

std::wstring QuoteArgument(const std::wstring &argument) {
    std::wstring quoted = L"\"";
    size_t trailingBackslashes = 0;

    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++trailingBackslashes;
            continue;
        }

        if (character == L'\"')
            quoted.append(trailingBackslashes * 2 + 1, L'\\');
        else
            quoted.append(trailingBackslashes, L'\\');

        quoted.push_back(character);
        trailingBackslashes = 0;
    }

    quoted.append(trailingBackslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

std::filesystem::path GetExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (length == 0)
            return {};

        if (length < buffer.size() - 1)
            return std::filesystem::path(std::wstring(buffer.data(), length));

        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path FindWindowsImage(const std::filesystem::path &executablePath) {
    std::vector<std::filesystem::path> candidates;
    const auto executableDirectory = executablePath.parent_path();

    candidates.push_back(executableDirectory / L"install.wim");
    candidates.push_back(executableDirectory / L"sources" / L"install.wim");

    std::error_code error;
    const auto currentDirectory = std::filesystem::current_path(error);

    if (!error) {
        candidates.push_back(currentDirectory / L"install.wim");
        candidates.push_back(currentDirectory / L"sources" / L"install.wim");
    }

    for (const auto &candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, error) && !error)
            return std::filesystem::absolute(candidate, error);
    }

    return {};
}

bool CopyOptionalPayloadDirectory(const std::filesystem::path &source, const std::filesystem::path &destination, std::ofstream &log, std::wstring &error) {
    std::error_code filesystemError;

    if (!std::filesystem::is_directory(source, filesystemError))
        return true;

    std::filesystem::create_directories(destination, filesystemError);

    if (filesystemError) {
        error = L"Unable to create the staged payload directory.";
        return false;
    }

    std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, filesystemError);

    if (filesystemError) {
        error = L"Unable to stage optional drivers or program installers.";
        return false;
    }

    log << "[INFO] Staged " << WideToUtf8(source.filename().wstring()) << " payload.\n";
    return true;
}

const wchar_t *ToArgumentMode(InstallMode mode) {
    switch (mode) {
    case InstallMode::ReinstallWindows:
        return L"reinstall_windows";

    case InstallMode::ReinstallWindowsAndFormatData:
        return L"reinstall_windows_and_format_data";

    case InstallMode::CleanDisk:
        return L"clean_disk";
    }

    return L"unknown";
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

bool IsRunningAsAdministrator() {
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = nullptr;

    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        return false;
    }

    BOOL isMember = FALSE;
    const BOOL succeeded = CheckTokenMembership(nullptr, administratorsGroup, &isMember);
    FreeSid(administratorsGroup);

    return succeeded && isMember == TRUE;
}

} // namespace

bool InstallationLauncher::Launch(const InstallerContext &context, std::wstring &error) {
    if (!context.SelectedDisk) {
        error = L"Выберите диск для установки.";
        return false;
    }

    if (context.SelectedEdition <= 0) {
        error = L"Выберите редакцию Windows.";
        return false;
    }

    if (!IsRunningAsAdministrator()) {
        error = L"Phoenix Installer должен быть запущен от имени администратора.";
        return false;
    }

    const auto executablePath = GetExecutablePath();

    if (executablePath.empty()) {
        error = L"Не удалось определить расположение Phoenix Installer.";
        return false;
    }

    const auto imagePath = FindWindowsImage(executablePath);

    if (imagePath.empty()) {
        error = L"install.wim не найден рядом с Phoenix Installer или в каталоге sources.";
        return false;
    }

    wchar_t temporaryPath[MAX_PATH]{};

    if (!GetTempPathW(ARRAYSIZE(temporaryPath), temporaryPath)) {
        error = L"Не удалось определить временный каталог.";
        return false;
    }

    const auto workspace = std::filesystem::path(temporaryPath) / (L"PhoenixInstaller-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code filesystemError;
    std::filesystem::create_directories(workspace / L"Logs", filesystemError);

    if (filesystemError) {
        error = L"Не удалось подготовить временный каталог Phoenix.";
        return false;
    }

    std::ofstream log(workspace / L"Logs" / L"install.log", std::ios::binary | std::ios::trunc);

    if (!log.is_open()) {
        error = L"Не удалось создать журнал установки.";
        return false;
    }

    log << "[INFO] Starting Phoenix Installer\n";
    log << "[INFO] Selected disk: " << context.SelectedDisk->Number << '\n';
    log << "[INFO] Selected disk model: " << WideToUtf8(context.SelectedDisk->Model) << '\n';
    log << "[INFO] Selected disk size: " << context.SelectedDisk->Size << " bytes\n";
    log << "[INFO] Windows edition: " << context.SelectedEdition << '\n';

    for (const auto &file : kPayloadFiles) {
        if (!ResourceManager::Extract(file.ResourceId, workspace / file.RelativePath)) {
            error = L"Не удалось извлечь встроенные файлы Phoenix.";
            return false;
        }
    }

    if (!ResourceManager::Extract(IDR_PHOENIX_SETUP, workspace / L"PhoenixSetup.exe")) {
        error = L"Не удалось извлечь PhoenixSetup.exe.";
        return false;
    }

    if (!CopyOptionalPayloadDirectory(executablePath.parent_path() / L"Drivers", workspace / L"Drivers", log, error) ||
        !CopyOptionalPayloadDirectory(executablePath.parent_path() / L"Programs", workspace / L"Programs", log, error)) {
        return false;
    }

    std::string configError;

    if (!InstallationConfig::Write(context, imagePath, workspace / L"config.json", configError)) {
        error = Utf8ToWide(configError);
        return false;
    }

    int windowsPartitionNumber = 0;
    int dataPartitionNumber = 0;

    if (context.InstallMode != InstallMode::CleanDisk) {
        const auto *windowsPartition = FindSinglePartition(*context.SelectedDisk, PartitionRole::Windows);

        if (!windowsPartition) {
            error = L"Не удалось однозначно определить раздел Windows на выбранном диске.";
            return false;
        }

        windowsPartitionNumber = static_cast<int>(windowsPartition->Number);

        if (context.InstallMode == InstallMode::ReinstallWindowsAndFormatData) {
            const auto *dataPartition = FindSinglePartition(*context.SelectedDisk, PartitionRole::Data);

            if (!dataPartition) {
                error = L"Не удалось однозначно определить раздел Data на выбранном диске.";
                return false;
            }

            dataPartitionNumber = static_cast<int>(dataPartition->Number);
        }
    }

    const uint64_t windowsSizeGb = context.WindowsPartitionSize / (1024ull * 1024ull * 1024ull);
    const auto installScript = workspace / L"Scripts" / L"Install.bat";

    std::wostringstream command;
    command << L"cmd.exe /d /k call " << QuoteArgument(installScript.wstring()) << ' ' << QuoteArgument(ToArgumentMode(context.InstallMode)) << ' ' << QuoteArgument(imagePath.wstring()) << ' '
            << context.SelectedEdition << ' ' << windowsSizeGb << ' ' << context.SelectedDisk->Number << ' ' << QuoteArgument(workspace.wstring()) << ' ' << windowsPartitionNumber << ' '
            << dataPartitionNumber;

    std::wstring commandLine = command.str();
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, workspace.c_str(), &startupInfo, &processInfo)) {
        error = L"Не удалось запустить сценарий установки Windows.";
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}
