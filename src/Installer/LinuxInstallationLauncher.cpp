#include "LinuxInstallationLauncher.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr const char *EFI_PARTTYPE = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b";

std::string QuoteForLog(const std::string &value) {
    return "'" + value + "'";
}

std::filesystem::path GetExecutablePath() {
    std::vector<char> buffer(4096);

    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);

    if (length <= 0)
        return {};

    buffer[length] = '\0';

    return std::filesystem::path(buffer.data());
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return value;
}

bool IsNtfs(const std::string &filesystem) {
    const std::string fs = ToLower(filesystem);

    return fs == "ntfs";
}

bool IsFat32(const std::string &filesystem) {
    const std::string fs = ToLower(filesystem);

    return fs == "vfat" || fs == "fat32";
}

bool IsEfiPartition(const std::string &filesystem, const std::string &label, const std::string &partType) {
    if (!partType.empty() && ToLower(partType) == EFI_PARTTYPE) {
        return true;
    }

    if (IsFat32(filesystem) && ToLower(label) == "efi") {
        return true;
    }

    return false;
}

} // namespace

std::string LinuxInstallationLauncher::NormalizeDevice(const std::string &device) {
    if (device.rfind("/dev/", 0) == 0)
        return device;

    return "/dev/" + device;
}

bool LinuxInstallationLauncher::IsUEFI() {
    return std::filesystem::exists("/sys/firmware/efi");
}

bool LinuxInstallationLauncher::RunCommand(const std::string &program, const std::vector<std::string> &arguments, std::string &error) {
    std::cout << "\n$ " << program;

    for (const auto &argument : arguments) {
        std::cout << ' ' << QuoteForLog(argument);
    }

    std::cout << '\n';

    std::vector<char *> argv;

    argv.reserve(arguments.size() + 2);

    argv.push_back(const_cast<char *>(program.c_str()));

    for (const auto &argument : arguments) {
        argv.push_back(const_cast<char *>(argument.c_str()));
    }

    argv.push_back(nullptr);

    const pid_t pid = fork();

    if (pid < 0) {
        error = "Не удалось выполнить fork().";

        return false;
    }

    if (pid == 0) {
        execvp(program.c_str(), argv.data());

        _exit(127);
    }

    int status = 0;

    while (true) {
        const pid_t result = waitpid(pid, &status, 0);

        if (result == pid)
            break;

        if (result < 0) {
            error = "Ошибка ожидания дочернего процесса.";

            return false;
        }
    }

    if (WIFEXITED(status)) {
        const int exitCode = WEXITSTATUS(status);

        if (exitCode == 0)
            return true;

        std::ostringstream stream;

        stream << "Команда завершилась с кодом " << exitCode << ": " << program;

        error = stream.str();

        return false;
    }

    if (WIFSIGNALED(status)) {
        std::ostringstream stream;

        stream << "Команда была завершена сигналом " << WTERMSIG(status) << ": " << program;

        error = stream.str();

        return false;
    }

    error = "Команда завершилась неизвестным способом.";

    return false;
}

bool LinuxInstallationLauncher::RunPrivilegedCommand(const std::string &program, const std::vector<std::string> &arguments, std::string &error) {
    std::vector<std::string> sudoArguments;

    sudoArguments.reserve(arguments.size() + 2);

    sudoArguments.push_back("-n");
    sudoArguments.push_back(program);

    for (const auto &argument : arguments) {
        sudoArguments.push_back(argument);
    }

    return RunCommand("sudo", sudoArguments, error);
}

bool LinuxInstallationLauncher::RunPrivilegedCommandWithInput(const std::string &program, const std::vector<std::string> &arguments, const std::string &input, std::string &error) {
    std::vector<std::string> sudoArguments;

    sudoArguments.reserve(arguments.size() + 2);

    sudoArguments.push_back("-n");
    sudoArguments.push_back(program);

    for (const auto &argument : arguments)
        sudoArguments.push_back(argument);

    std::vector<char *> argv;

    argv.reserve(sudoArguments.size() + 2);

    argv.push_back(const_cast<char *>("sudo"));

    for (auto &argument : sudoArguments)
        argv.push_back(const_cast<char *>(argument.c_str()));

    argv.push_back(nullptr);

    int pipeFd[2];

    if (pipe(pipeFd) != 0) {
        error = "Не удалось создать pipe().";
        return false;
    }

    const pid_t pid = fork();

    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);

        error = "Не удалось выполнить fork().";
        return false;
    }

    if (pid == 0) {
        close(pipeFd[1]);

        if (dup2(pipeFd[0], STDIN_FILENO) < 0)
            _exit(127);

        close(pipeFd[0]);

        execvp("sudo", argv.data());

        _exit(127);
    }

    close(pipeFd[0]);

    std::size_t totalWritten = 0;

    while (totalWritten < input.size()) {
        const ssize_t written = write(pipeFd[1], input.data() + totalWritten, input.size() - totalWritten);

        if (written > 0) {
            totalWritten += static_cast<std::size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        close(pipeFd[1]);

        error = "Ошибка записи данных в stdin команды.";
        return false;
    }

    close(pipeFd[1]);

    int status = 0;

    while (true) {
        const pid_t result = waitpid(pid, &status, 0);

        if (result == pid)
            break;

        if (result < 0) {
            error = "Ошибка ожидания дочернего процесса.";
            return false;
        }
    }

    if (WIFEXITED(status)) {
        const int exitCode = WEXITSTATUS(status);

        if (exitCode == 0)
            return true;

        std::ostringstream stream;

        stream << "Команда завершилась с кодом " << exitCode << ": " << program;

        error = stream.str();

        return false;
    }

    if (WIFSIGNALED(status)) {
        std::ostringstream stream;

        stream << "Команда была завершена сигналом " << WTERMSIG(status) << ": " << program;

        error = stream.str();

        return false;
    }

    error = "Команда завершилась неизвестным способом.";

    return false;
}

bool LinuxInstallationLauncher::CreatePostInstallConfig(const InstallerContext &context, const std::filesystem::path &configPath, std::string &error) {
    using json = nlohmann::json;

    json config;

    config["computer"] = {{"change_name", context.ChangeComputerName}, {"name", context.ComputerName}};

    config["packages"] = json::array();
    config["drivers"] = context.InstallDrivers;

    for (const std::string &packageId : context.SelectedPackages) {
        const Package *package = context.Packages.FindById(packageId);

        if (!package) {
            error = "Не найден пакет в PackageManager:\n\n" + packageId;

            return false;
        }

        json packageConfig;

        packageConfig["id"] = package->Id;
        packageConfig["folder"] = package->Folder;
        packageConfig["installer"] = package->Installer;
        packageConfig["arguments"] = package->Arguments;

        config["packages"].push_back(std::move(packageConfig));
    }

    std::ofstream stream(configPath);

    if (!stream.is_open()) {
        error = "Не удалось создать config.json:\n\n" + configPath.string();

        return false;
    }

    stream << config.dump(4) << '\n';

    if (!stream.good()) {
        error = "Ошибка записи config.json:\n\n" + configPath.string();

        return false;
    }

    return true;
}

bool RunPrivilegedCommandWithProgress(const std::string &program, const std::vector<std::string> &arguments, const std::function<void(float)> &progressCallback, std::string &error) {
    std::cout << "\n$ sudo -n " << program;

    for (const auto &argument : arguments)
        std::cout << ' ' << QuoteForLog(argument);

    std::cout << '\n';

    int pipeFd[2];

    if (pipe(pipeFd) != 0) {
        error = "Не удалось создать pipe().";
        return false;
    }

    std::vector<std::string> sudoArguments;

    sudoArguments.reserve(arguments.size() + 2);

    sudoArguments.push_back("-n");
    sudoArguments.push_back(program);

    for (const auto &argument : arguments)
        sudoArguments.push_back(argument);

    std::vector<char *> argv;

    argv.reserve(sudoArguments.size() + 2);

    argv.push_back(const_cast<char *>("sudo"));

    for (auto &argument : sudoArguments)
        argv.push_back(const_cast<char *>(argument.c_str()));

    argv.push_back(nullptr);

    const pid_t pid = fork();

    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);

        error = "Не удалось выполнить fork().";
        return false;
    }

    if (pid == 0) {
        close(pipeFd[0]);

        dup2(pipeFd[1], STDOUT_FILENO);
        dup2(pipeFd[1], STDERR_FILENO);

        close(pipeFd[1]);

        execvp("sudo", argv.data());

        _exit(127);
    }

    close(pipeFd[1]);

    std::string output;

    char buffer[4096];

    while (true) {
        const ssize_t count = read(pipeFd[0], buffer, sizeof(buffer));

        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));

            /*
             * wimlib использует \r для обновления
             * одной строки прогресса.
             */
            std::size_t start = 0;

            while (start < output.size()) {
                const std::size_t end = output.find_first_of("\r\n", start);

                if (end == std::string::npos)
                    break;

                const std::string line = output.substr(start, end - start);

                const std::size_t percentPos = line.find('%');

                if (percentPos != std::string::npos) {
                    std::size_t numberStart = percentPos;

                    while (numberStart > 0 && std::isdigit(static_cast<unsigned char>(line[numberStart - 1]))) {
                        --numberStart;
                    }

                    if (numberStart < percentPos) {
                        try {
                            const int percent = std::stoi(line.substr(numberStart, percentPos - numberStart));

                            if (percent >= 0 && percent <= 100) {
                                if (progressCallback)
                                    progressCallback(static_cast<float>(percent) / 100.0f);
                            }
                        }
                        catch (...) {
                        }
                    }
                }

                start = end + 1;
            }

            if (start > 0)
                output.erase(0, start);

            continue;
        }

        if (count == 0)
            break;

        if (errno == EINTR)
            continue;

        close(pipeFd[0]);

        error = "Ошибка чтения вывода команды.";
        return false;
    }

    close(pipeFd[0]);

    int status = 0;

    while (true) {
        const pid_t result = waitpid(pid, &status, 0);

        if (result == pid)
            break;

        if (result < 0) {
            error = "Ошибка ожидания дочернего процесса.";
            return false;
        }
    }

    std::cout << output;

    if (WIFEXITED(status)) {
        const int exitCode = WEXITSTATUS(status);

        if (exitCode == 0)
            return true;

        std::ostringstream stream;

        stream << "Команда завершилась с кодом " << exitCode << ": " << program;

        error = stream.str();

        return false;
    }

    if (WIFSIGNALED(status)) {
        std::ostringstream stream;

        stream << "Команда была завершена сигналом " << WTERMSIG(status) << ": " << program;

        error = stream.str();

        return false;
    }

    error = "Команда завершилась неизвестным способом.";

    return false;
}

bool LinuxInstallationLauncher::RunCommandCapture(const std::string &program, const std::vector<std::string> &arguments, std::string &output, std::string &error) {
    std::cout << "\n$ " << program;

    for (const auto &argument : arguments) {
        std::cout << ' ' << QuoteForLog(argument);
    }

    std::cout << '\n';

    int pipeFd[2];

    if (pipe(pipeFd) != 0) {
        error = "Не удалось создать pipe().";

        return false;
    }

    std::vector<char *> argv;

    argv.reserve(arguments.size() + 2);

    argv.push_back(const_cast<char *>(program.c_str()));

    for (const auto &argument : arguments) {
        argv.push_back(const_cast<char *>(argument.c_str()));
    }

    argv.push_back(nullptr);

    const pid_t pid = fork();

    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);

        error = "Не удалось выполнить fork().";

        return false;
    }

    if (pid == 0) {
        close(pipeFd[0]);

        dup2(pipeFd[1], STDOUT_FILENO);

        dup2(pipeFd[1], STDERR_FILENO);

        close(pipeFd[1]);

        execvp(program.c_str(), argv.data());

        _exit(127);
    }

    close(pipeFd[1]);

    output.clear();

    char buffer[4096];

    while (true) {
        const ssize_t count = read(pipeFd[0], buffer, sizeof(buffer));

        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));

            continue;
        }

        if (count == 0)
            break;

        if (errno == EINTR)
            continue;

        close(pipeFd[0]);

        error = "Ошибка чтения вывода команды.";

        return false;
    }

    close(pipeFd[0]);

    int status = 0;

    while (true) {
        const pid_t result = waitpid(pid, &status, 0);

        if (result == pid)
            break;

        if (result < 0) {
            error = "Ошибка ожидания дочернего процесса.";

            return false;
        }
    }

    std::cout << output;

    if (WIFEXITED(status)) {
        const int exitCode = WEXITSTATUS(status);

        if (exitCode == 0)
            return true;

        std::ostringstream stream;

        stream << "Команда завершилась с кодом " << exitCode << ": " << program;

        error = stream.str();

        return false;
    }

    if (WIFSIGNALED(status)) {
        std::ostringstream stream;

        stream << "Команда была завершена сигналом " << WTERMSIG(status) << ": " << program;

        error = stream.str();

        return false;
    }

    error = "Команда завершилась неизвестным способом.";

    return false;
}

bool LinuxInstallationLauncher::GetExecutableDirectory(std::filesystem::path &directory, std::string &error) {
    const auto executable = GetExecutablePath();

    if (executable.empty()) {
        error = "Не удалось определить путь к Installer.";

        return false;
    }

    directory = executable.parent_path();

    if (directory.empty()) {
        error = "Не удалось определить каталог Installer.";

        return false;
    }

    return true;
}

bool LinuxInstallationLauncher::ParseLsblkOutput(const std::string &output, std::vector<BlockPartition> &partitions, std::string &error) {
    partitions.clear();

    std::istringstream stream(output);

    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty())
            continue;

        std::istringstream lineStream(line);

        BlockPartition partition;

        lineStream >> partition.device >> partition.type >> partition.filesystem >> partition.label >> partition.partType;

        if (partition.device.empty())
            continue;

        if (partition.type != "part")
            continue;

        partitions.push_back(std::move(partition));
    }

    if (partitions.empty()) {
        error = "lsblk не вернул ни одного раздела.";

        return false;
    }

    return true;
}

bool LinuxInstallationLauncher::FindExistingPartitions(const InstallerContext &context, ExistingPartitions &partitions, std::string &error) {
    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);

    partitions = {};

    std::string output;

    if (!RunCommandCapture("lsblk", {"-rpn", "-o", "NAME,TYPE,FSTYPE,LABEL,PARTTYPE", disk}, output, error)) {
        return false;
    }

    std::vector<BlockPartition> blockPartitions;

    if (!ParseLsblkOutput(output, blockPartitions, error)) {
        return false;
    }

    for (const auto &partition : blockPartitions) {
        if (partitions.efi.empty() && IsEfiPartition(partition.filesystem, partition.label, partition.partType)) {
            partitions.efi = partition.device;
            continue;
        }

        if (!IsNtfs(partition.filesystem))
            continue;

        const std::string label = ToLower(partition.label);

        if (partitions.windows.empty() && label == "windows") {
            partitions.windows = partition.device;
            continue;
        }

        if (partitions.data.empty() && label == "data") {
            partitions.data = partition.device;
            continue;
        }
    }

    if (partitions.windows.empty()) {
        error = "Раздел Windows (C:) не найден.\n\n"
                "Невозможно безопасно выполнить "
                "переустановку Windows.";

        return false;
    }

    return true;
}

bool LinuxInstallationLauncher::Install(const InstallerContext &context, std::string &error, ProgressCallback progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };

    ReportProgress(0.0f, "Подготовка установки...");

    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const bool uefi = IsUEFI();

    ReportProgress(0.02f, uefi ? "Режим загрузки: UEFI" : "Режим загрузки: Legacy BIOS");

    if (context.Mode == InstallMode::ReinstallWindows) {
        ReportProgress(0.05f, "Подготовка переустановки Windows...");

        if (uefi) {
            return InstallExistingWindows(context, error, progress);
        }

        return InstallLegacy(context, error, progress);
    }

    if (context.Mode == InstallMode::ReinstallWindowsAndFormatData) {
        ReportProgress(0.05f, "Подготовка переустановки Windows и форматирования Data...");

        if (uefi) {
            return InstallCleanDisk(context, error, progress);
        }

        InstallerContext legacyContext = context;
        legacyContext.WindowsPartitionSize = 120ull * 1024ull * 1024ull * 1024ull;
        return InstallLegacyCleanDisk(legacyContext, error, progress);
    }

    if (context.Mode == InstallMode::CleanDisk) {
        ReportProgress(0.05f, "Подготовка полной очистки диска...");

        if (uefi) {
            return InstallCleanDisk(context, error, progress);
        }

        return InstallLegacyCleanDisk(context, error, progress);
    }

    error = "Неизвестный режим установки.";
    return false;
}

bool LinuxInstallationLauncher::InstallUEFI(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    switch (context.Mode) {
    case InstallMode::ReinstallWindows:
        return InstallExistingWindows(context, error, progress);

    case InstallMode::ReinstallWindowsAndFormatData:
        return InstallCleanDisk(context, error, progress);

    case InstallMode::CleanDisk:
        return InstallCleanDisk(context, error, progress);
    }

    error = "Неизвестный режим установки.";

    return false;
}

bool LinuxInstallationLauncher::InstallExistingWindows(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };
    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);

    ExistingPartitions partitions;

    ReportProgress(0.06f, "Проверка разделов...");

    if (!FindExistingPartitions(context, partitions, error)) {
        return false;
    }

    if (partitions.efi.empty()) {
        error = "EFI-раздел не найден.\n\n"
                "Для установки Windows в режиме UEFI "
                "необходим существующий EFI-раздел.";

        return false;
    }

    if (partitions.windows.empty()) {
        error = "Раздел Windows (C:) не найден.\n\n"
                "Невозможно безопасно выполнить "
                "переустановку Windows.";

        return false;
    }

    std::cout << "\n========================================\n"
              << " Reinstall Windows\n"
              << "========================================\n"
              << "Disk:      " << disk << '\n'
              << "EFI:       " << partitions.efi << '\n'
              << "Windows:   " << partitions.windows << '\n'
              << "Data:      ";

    if (partitions.data.empty())
        std::cout << "not found";
    else
        std::cout << partitions.data;

    std::cout << '\n';

    /*
     * Проверяем, что Windows-раздел не смонтирован.
     *
     * Это важно: WIM накладывается непосредственно
     * на block device, поэтому NTFS не должен быть
     * смонтирован в этот момент.
     */
    std::string mountOutput;
    std::string mountError;

    if (RunCommandCapture("findmnt", {"-rn", "-S", partitions.windows}, mountOutput, mountError)) {
        error = "Раздел Windows уже смонтирован:\n\n" + mountOutput +
                "\nПеред установкой его необходимо "
                "размонтировать.";

        return false;
    }

    /*
     * Проверяем наличие ISO.
     */
    /*============================================================================================*/
    const std::filesystem::path isoPath = "/Windows10.iso";

    std::error_code isoError;

    const bool isoExists = std::filesystem::exists(isoPath, isoError);

    if (!isoExists) {
        if (isoError) {
            error = "Не удалось проверить ISO Windows:\n\n" + isoPath.string() + "\n\nОшибка: " + isoError.message();
        }
        else {
            error = "Не найден ISO Windows:\n\n" + isoPath.string();
        }

        return false;
    }
    /*============================================================================================*/

    const std::filesystem::path isoMount = "/mnt/winiso";

    const std::filesystem::path windowsMount = "/mnt/windows";

    const std::filesystem::path efiMount = "/mnt/efi";

    if (!RunPrivilegedCommand("mkdir", {"-p", isoMount.string(), windowsMount.string(), efiMount.string()}, error)) {
        return false;
    }

    /*
     * На всякий случай очищаем старые mount points.
     * Ошибки здесь игнорируем: если они не были
     * смонтированы, umount просто завершится ошибкой.
     */
    std::string cleanupError;

    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

    RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

    RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

    /*
     * Форматируем ТОЛЬКО Windows.
     *
     * EFI не форматируем.
     * Data не форматируем.
     */
    std::cout << "\nФорматирование Windows-раздела...\n";

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Windows", partitions.windows}, error)) {
        return false;
    }

    /*
     * Монтируем ISO.
     */
    std::cout << "\nМонтирование Windows ISO...\n";

    if (!RunPrivilegedCommand("mount", {"-o", "loop,ro", isoPath.string(), isoMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path installWim = isoMount / "sources" / "install.wim";

    if (!std::filesystem::exists(installWim)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

        error = "В ISO не найден:\n\n" + installWim.string();

        return false;
    }

    /*
     * КРИТИЧЕСКИЙ МОМЕНТ:
     *
     * WIM накладывается непосредственно
     * на block device Windows-раздела.
     *
     * НЕ монтируем NTFS перед apply.
     */
    std::cout << "\nПрименение Windows image (index 2)...\n";

    ReportProgress(0.30f, "Установка Windows...");

    if (!RunPrivilegedCommandWithProgress(
            "wimlib-imagex", {"apply", installWim.string(), "2", partitions.windows},
            [&](float wimProgress) {
                const float globalProgress = 0.30f + wimProgress * 0.50f;

                ReportProgress(globalProgress, "Установка Windows... " + std::to_string(static_cast<int>(wimProgress * 100.0f)) + "%");
            },
            error)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

        return false;
    }

    /*
     * ISO больше не нужен.
     */
    if (!RunPrivilegedCommand("umount", {isoMount.string()}, error)) {
        return false;
    }

    /*
     * Теперь монтируем готовый Windows-раздел.
     */
    std::cout << "\nМонтирование Windows-раздела...\n";

    if (!RunPrivilegedCommand("mount", {"-t", "ntfs3", partitions.windows, windowsMount.string()}, error)) {
        return false;
    }

    /*
     * Проверяем, что Windows действительно
     * была применена.
     */
    const std::filesystem::path kernel = windowsMount / "Windows" / "System32" / "ntoskrnl.exe";

    if (!std::filesystem::exists(kernel)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Windows image была применена, "
                "но не найден:\n\n" +
                kernel.string();

        return false;
    }

    std::cout << "\nWindows image успешно применена.\n";

    /*
     * Монтируем EFI.
     */
    std::cout << "\nМонтирование EFI-раздела...\n";

    if (!RunPrivilegedCommand("mount", {"-t", "vfat", partitions.efi, efiMount.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    /*
     * Находим bcd-sys рядом с Installer.
     */
    std::filesystem::path executableDirectory;

    if (!GetExecutableDirectory(executableDirectory, error)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path bcdSys = executableDirectory / "bcd-sys-2.4-x86_64.AppImage";

    if (!std::filesystem::exists(bcdSys)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден BCD-SYS:\n\n" + bcdSys.string();

        return false;
    }

    /*
     * Настраиваем загрузчик Windows для UEFI.
     */
    std::cout << "\nНастройка Windows Boot Manager...\n";

    if (!RunPrivilegedCommand("env",
                              {"LC_ALL=C", bcdSys.string(), windowsMount.string(), "--firmware", "UEFI", "--syspath", efiMount.string(), "--clean", "--locale", "ru-RU", "--prodname",
                               "Windows 10 Enterprise", "--verbose"},
                              error)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    /*
     * Финальная проверка UEFI-загрузчика.
     *
     * Для UEFI boot manager находится на EFI System Partition:
     *
     * EFI/Microsoft/Boot/bootmgfw.efi
     * EFI/Microsoft/Boot/BCD
     *
     * C:\bootmgr для UEFI здесь не проверяем.
     */
    const std::filesystem::path bootManager = efiMount / "EFI" / "Microsoft" / "Boot" / "bootmgfw.efi";

    const std::filesystem::path bcd = efiMount / "EFI" / "Microsoft" / "Boot" / "BCD";

    if (!std::filesystem::exists(bootManager)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден UEFI Windows Boot Manager:\n\n" + bootManager.string();

        return false;
    }

    if (!std::filesystem::exists(bcd)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден UEFI BCD:\n\n" + bcd.string();

        return false;
    }

    /*
     * Всё успешно — размонтируем.
     */
    std::cout << "\nПроверка UEFI-загрузчика прошла успешно.\n";

    if (!RunPrivilegedCommand("umount", {efiMount.string()}, error)) {
        return false;
    }

    ReportProgress(0.97f, "Установка файлов первого запуска...");

    const std::filesystem::path scriptsDirectory = executableDirectory / "assets" / "Scripts";

    const std::filesystem::path unattend = scriptsDirectory / "unattend.xml";

    const std::filesystem::path setupComplete = scriptsDirectory / "SetupComplete.cmd";

    if (!std::filesystem::exists(unattend)) {
        error = "Не найден unattend.xml:\n\n" + unattend.string();

        return false;
    }

    if (!std::filesystem::exists(setupComplete)) {
        error = "Не найден SetupComplete.cmd:\n\n" + setupComplete.string();

        return false;
    }

    const std::filesystem::path pantherDirectory = windowsMount / "Windows" / "Panther";

    const std::filesystem::path setupScriptsDirectory = windowsMount / "Windows" / "Setup" / "Scripts";

    const std::filesystem::path postInstallDirectory = windowsMount / "Programs" / "PostInstall";

    const std::filesystem::path startupDirectory = windowsMount / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";

    const std::filesystem::path psetup = executableDirectory / "assets" / "PSetup.exe";

    if (!std::filesystem::exists(psetup)) {
        error = "Не найден PSetup.exe:\n\n" + psetup.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path sysprepDirectory = windowsMount / "Windows" / "System32" / "Sysprep";

    if (!RunPrivilegedCommand("mkdir", {"-p", pantherDirectory.string(), sysprepDirectory.string(), setupScriptsDirectory.string(), postInstallDirectory.string(), startupDirectory.string()}, error)) {
        return false;
    }

    const std::filesystem::path temporaryConfig = "/tmp/PhoenixInstaller-config.json";

    if (!CreatePostInstallConfig(context, temporaryConfig, error)) {
        return false;
    }

    const std::filesystem::path postInstallConfig = postInstallDirectory / "config.json";

    if (!RunPrivilegedCommand("cp", {temporaryConfig.string(), postInstallConfig.string()}, error)) {
        std::error_code removeError;
        std::filesystem::remove(temporaryConfig, removeError);

        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(temporaryConfig, removeError);

    if (!std::filesystem::exists(postInstallConfig)) {
        error = "config.json не был скопирован:\n\n" + postInstallConfig.string();

        return false;
    }

    const std::filesystem::path postInstallPsetup = postInstallDirectory / "PSetup.exe";

    if (!RunPrivilegedCommand("cp", {psetup.string(), postInstallPsetup.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryPsetupCmd = "/tmp/PSetup.cmd";

    {
        std::ofstream stream(temporaryPsetupCmd);

        if (!stream.is_open()) {
            error = "Не удалось создать временный PSetup.cmd.";
            RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
            return false;
        }

        stream << "@echo off\r\n"
               << "start \"\" \"C:\\Programs\\PostInstall\\PSetup.exe\"\r\n";
    }

    const std::filesystem::path startupPsetupCmd = startupDirectory / "PSetup.cmd";

    if (!RunPrivilegedCommand("cp", {temporaryPsetupCmd.string(), startupPsetupCmd.string()}, error)) {
        std::error_code removePsetupError;
        std::filesystem::remove(temporaryPsetupCmd, removePsetupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removePsetupError;
    std::filesystem::remove(temporaryPsetupCmd, removePsetupError);

    if (!RunPrivilegedCommand("cp", {unattend.string(), (pantherDirectory / "unattend.xml").string()}, error)) {
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (sysprepDirectory / "unattend.xml").string()}, error)) {
        return false;
    }

    /*
     * SetupComplete.cmd пока НЕ копируем.
     * Оставляем код здесь закомментированным для последующего включения.
     */
    // if (!RunPrivilegedCommand("cp", {setupComplete.string(), (setupScriptsDirectory / "SetupComplete.cmd").string()}, error)) {
    //     return false;
    // }

    if (!std::filesystem::exists(pantherDirectory / "unattend.xml")) {
        error = "unattend.xml не был скопирован:\n\n" + (pantherDirectory / "unattend.xml").string();

        return false;
    }

    // if (!std::filesystem::exists(setupScriptsDirectory / "SetupComplete.cmd")) {
    //     error = "SetupComplete.cmd не был скопирован:\n\n" + (setupScriptsDirectory / "SetupComplete.cmd").string();

    //     return false;
    // }

    ReportProgress(0.99f, "Завершение установки...");

    if (!RunPrivilegedCommand("umount", {windowsMount.string()}, error)) {
        return false;
    }

    ReportProgress(1.0f, "Установка Windows завершена.");

    std::cout << "\n========================================\n"
              << " Windows успешно установлена\n"
              << "========================================\n";

    return true;
}

bool LinuxInstallationLauncher::InstallExistingWindowsAndData(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };

    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);

    ExistingPartitions partitions;

    ReportProgress(0.06f, "Проверка разделов Windows и Data...");

    if (!FindExistingPartitions(context, partitions, error)) {
        return false;
    }

    if (partitions.efi.empty()) {
        error = "EFI-раздел не найден.\n\n"
                "Для установки Windows в режиме UEFI "
                "необходим существующий EFI-раздел.";
        return false;
    }

    if (partitions.windows.empty()) {
        error = "Раздел Windows (C:) не найден.\n\n"
                "Невозможно безопасно выполнить переустановку Windows.";
        return false;
    }

    if (partitions.data.empty()) {
        error = "Раздел D: не найден.\n\n"
                "Для выбранного режима установки необходим "
                "существующий раздел данных.";
        return false;
    }

    std::cout << "\n========================================\n"
              << " Reinstall Windows + Format Data\n"
              << "========================================\n"
              << "Disk:      " << disk << '\n'
              << "EFI:       " << partitions.efi << '\n'
              << "Windows:   " << partitions.windows << '\n'
              << "Data:      " << partitions.data << '\n'
              << "\nБудут изменены только Windows и Data.\n"
              << "EFI останется без изменений.\n"
              << "AMDZ, если существует, останется без изменений.\n"
              << "========================================\n";

    /*
     * Windows и Data не должны быть смонтированы.
     * WIM применяется непосредственно к block device.
     */
    ReportProgress(0.08f, "Проверка Windows-раздела...");

    std::string mountOutput;
    std::string mountError;

    if (RunCommandCapture("findmnt", {"-rn", "-S", partitions.windows}, mountOutput, mountError)) {
        error = "Раздел Windows уже смонтирован:\n\n" + mountOutput + "\nПеред установкой его необходимо размонтировать.";
        return false;
    }

    if (RunCommandCapture("findmnt", {"-rn", "-S", partitions.data}, mountOutput, mountError)) {
        error = "Раздел Data уже смонтирован:\n\n" + mountOutput + "\nПеред установкой его необходимо размонтировать.";
        return false;
    }

    /*
     * Проверяем ISO Windows.
     */
    const std::filesystem::path isoPath = "/Windows10.iso";

    std::error_code isoError;

    if (!std::filesystem::exists(isoPath, isoError)) {
        if (isoError) {
            error = "Не удалось проверить ISO Windows:\n\n" + isoPath.string() + "\n\nОшибка: " + isoError.message();
        }
        else {
            error = "Не найден ISO Windows:\n\n" + isoPath.string();
        }
        return false;
    }

    const std::filesystem::path isoMount = "/mnt/winiso";
    const std::filesystem::path windowsMount = "/mnt/windows";
    const std::filesystem::path efiMount = "/mnt/efi";

    if (!RunPrivilegedCommand("mkdir", {"-p", isoMount.string(), windowsMount.string(), efiMount.string()}, error)) {
        return false;
    }

    std::string cleanupError;

    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
    RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

    /*
     * Форматируем только Windows и Data.
     * EFI не трогаем.
     * Recovery не трогаем.
     * AMDZ не трогаем.
     */
    ReportProgress(0.12f, "Форматирование Windows-раздела...");

    std::cout << "\nФорматирование Windows-раздела...\n";

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Windows", partitions.windows}, error)) {
        return false;
    }

    ReportProgress(0.16f, "Форматирование Data-раздела...");

    std::cout << "\nФорматирование Data-раздела...\n";

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Data", partitions.data}, error)) {
        return false;
    }

    /*
     * Монтируем ISO.
     */
    ReportProgress(0.18f, "Монтирование Windows ISO...");

    if (!RunPrivilegedCommand("mount", {"-o", "loop,ro", isoPath.string(), isoMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path installWim = isoMount / "sources" / "install.wim";

    if (!std::filesystem::exists(installWim)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        error = "В ISO не найден:\n\n" + installWim.string();
        return false;
    }

    /*
     * КРИТИЧЕСКИ ВАЖНО:
     * Windows-раздел НЕ монтируем перед WIM apply.
     *
     * WIM применяется непосредственно к block device.
     */
    ReportProgress(0.20f, "Установка Windows...");

    std::cout << "\nПрименение Windows image (index 2)...\n";

    if (!RunPrivilegedCommandWithProgress(
            "wimlib-imagex", {"apply", installWim.string(), "2", partitions.windows},
            [&](float wimProgress) {
                const float globalProgress = 0.20f + wimProgress * 0.60f;

                ReportProgress(globalProgress, "Установка Windows... " + std::to_string(static_cast<int>(wimProgress * 100.0f)) + "%");
            },
            error)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        return false;
    }

    /*
     * ISO больше не нужен.
     */
    if (!RunPrivilegedCommand("umount", {isoMount.string()}, error)) {
        return false;
    }

    /*
     * Теперь монтируем готовый Windows-раздел.
     */
    ReportProgress(0.82f, "Проверка установленной Windows...");

    if (!RunPrivilegedCommand("mount", {"-t", "ntfs3", partitions.windows, windowsMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path kernel = windowsMount / "Windows" / "System32" / "ntoskrnl.exe";

    if (!std::filesystem::exists(kernel)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Windows image была применена, но не найден:\n\n" + kernel.string();
        return false;
    }

    std::cout << "\nWindows image успешно применена.\n";

    /*
     * Монтируем существующий EFI-раздел.
     * Он НЕ форматируется.
     */
    ReportProgress(0.84f, "Настройка UEFI-загрузчика...");

    if (!RunPrivilegedCommand("mount", {"-t", "vfat", partitions.efi, efiMount.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * Находим BCD-SYS рядом с Installer.
     */
    std::filesystem::path executableDirectory;

    if (!GetExecutableDirectory(executableDirectory, error)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path bcdSys = executableDirectory / "bcd-sys-2.4-x86_64.AppImage";

    if (!std::filesystem::exists(bcdSys)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден BCD-SYS:\n\n" + bcdSys.string();
        return false;
    }

    /*
     * Настраиваем Windows Boot Manager на существующем EFI.
     */
    std::cout << "\nНастройка Windows Boot Manager...\n";

    if (!RunPrivilegedCommand("env",
                              {"LC_ALL=C", bcdSys.string(), windowsMount.string(), "--firmware", "UEFI", "--syspath", efiMount.string(), "--clean", "--locale", "ru-RU", "--prodname",
                               "Windows 10 Enterprise", "--verbose"},
                              error)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * Проверяем UEFI Boot Manager и BCD.
     */
    const std::filesystem::path bootManager = efiMount / "EFI" / "Microsoft" / "Boot" / "bootmgfw.efi";

    const std::filesystem::path bcd = efiMount / "EFI" / "Microsoft" / "Boot" / "BCD";

    if (!std::filesystem::exists(bootManager)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден UEFI Windows Boot Manager:\n\n" + bootManager.string();
        return false;
    }

    if (!std::filesystem::exists(bcd)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден UEFI Windows BCD:\n\n" + bcd.string();
        return false;
    }

    std::cout << "\nПроверка UEFI-загрузчика прошла успешно.\n";

    if (!RunPrivilegedCommand("umount", {efiMount.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * Пока сохраняем рабочую схему:
     * unattend.xml и SetupComplete.cmd НЕ копируем.
     * Создаём только config.json.
     */
    ReportProgress(0.94f, "Создание конфигурации PostInstall...");

    const std::filesystem::path scriptsDirectory = executableDirectory / "assets" / "Scripts";

    const std::filesystem::path unattend = scriptsDirectory / "unattend.xml";

    const std::filesystem::path setupComplete = scriptsDirectory / "SetupComplete.cmd";

    if (!std::filesystem::exists(unattend)) {
        error = "Не найден unattend.xml:\n\n" + unattend.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!std::filesystem::exists(setupComplete)) {
        error = "Не найден SetupComplete.cmd:\n\n" + setupComplete.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path pantherDirectory = windowsMount / "Windows" / "Panther";

    const std::filesystem::path setupScriptsDirectory = windowsMount / "Windows" / "Setup" / "Scripts";

    const std::filesystem::path sysprepDirectory = windowsMount / "Windows" / "System32" / "Sysprep";

    if (!RunPrivilegedCommand("mkdir", {"-p", pantherDirectory.string(), sysprepDirectory.string(), setupScriptsDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (pantherDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (sysprepDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * SetupComplete.cmd пока НЕ копируем.
     * Код оставлен закомментированным для последующего включения.
     */
    // if (!RunPrivilegedCommand(
    //         "cp", {setupComplete.string(), (setupScriptsDirectory / "SetupComplete.cmd").string()}, error)) {
    //     RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    //     return false;
    // }

    const std::filesystem::path postInstallDirectory = windowsMount / "Programs" / "PostInstall";

    const std::filesystem::path startupDirectory = windowsMount / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";

    const std::filesystem::path psetup = executableDirectory / "assets" / "PSetup.exe";

    if (!std::filesystem::exists(psetup)) {
        error = "Не найден PSetup.exe:\n\n" + psetup.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!RunPrivilegedCommand("mkdir", {"-p", postInstallDirectory.string(), startupDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryConfig = "/tmp/PhoenixInstaller-config.json";

    if (!CreatePostInstallConfig(context, temporaryConfig, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallConfig = postInstallDirectory / "config.json";

    if (!RunPrivilegedCommand("cp", {temporaryConfig.string(), postInstallConfig.string()}, error)) {
        std::error_code removeError;
        std::filesystem::remove(temporaryConfig, removeError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(temporaryConfig, removeError);

    if (!std::filesystem::exists(postInstallConfig)) {
        error = "config.json не был скопирован:\n\n" + postInstallConfig.string();
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallPsetup = postInstallDirectory / "PSetup.exe";

    if (!RunPrivilegedCommand("cp", {psetup.string(), postInstallPsetup.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryPsetupCmd = "/tmp/PSetup.cmd";

    {
        std::ofstream stream(temporaryPsetupCmd);

        if (!stream.is_open()) {
            error = "Не удалось создать временный PSetup.cmd.";
            RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
            return false;
        }

        stream << "@echo off\r\n"
               << "start \"\" \"C:\\Programs\\PostInstall\\PSetup.exe\"\r\n";
    }

    const std::filesystem::path startupPsetupCmd = startupDirectory / "PSetup.cmd";

    if (!RunPrivilegedCommand("cp", {temporaryPsetupCmd.string(), startupPsetupCmd.string()}, error)) {
        std::error_code removePsetupError;
        std::filesystem::remove(temporaryPsetupCmd, removePsetupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removePsetupError;
    std::filesystem::remove(temporaryPsetupCmd, removePsetupError);

    ReportProgress(0.99f, "Завершение установки...");

    if (!RunPrivilegedCommand("umount", {windowsMount.string()}, error)) {
        return false;
    }

    ReportProgress(1.0f, "Windows и Data успешно переустановлены.");

    std::cout << "\n========================================\n"
              << " Windows успешно установлена\n"
              << " Data успешно отформатирован\n"
              << " EFI сохранён\n"
              << " AMDZ сохранён, если существовал\n"
              << "========================================\n";

    return true;
}

bool LinuxInstallationLauncher::InstallCleanDisk(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };

    constexpr std::uint64_t SECTOR_SIZE = 512;
    constexpr std::uint64_t GiB = 1024ull * 1024ull * 1024ull;
    constexpr std::uint64_t MiB = 1024ull * 1024ull;
    constexpr std::uint64_t EFI_SECTORS = (512ull * MiB) / SECTOR_SIZE;
    constexpr std::uint64_t MSR_SECTORS = (16ull * MiB) / SECTOR_SIZE;
    constexpr std::uint64_t FIRST_LBA = 2048;
    constexpr std::uint64_t ALIGNMENT_SECTORS = 2048;
    constexpr std::uint64_t GPT_BACKUP_SECTORS = 33;
    constexpr std::uint64_t AMDZ_DETECTION_RANGE_SECTORS = (3ull * GiB) / SECTOR_SIZE;
    constexpr std::uint64_t MIN_PRESERVED_SYSTEM_SECTORS = (64ull * MiB) / SECTOR_SIZE;
    constexpr std::uint64_t MAX_PRESERVED_SYSTEM_SECTORS = (192ull * MiB) / SECTOR_SIZE;
    constexpr std::uint64_t ADJACENCY_TOLERANCE_SECTORS = (1ull * MiB) / SECTOR_SIZE;
    constexpr std::uint64_t MIN_DATA_SECTORS = (1024ull * MiB) / SECTOR_SIZE;

    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);
    const std::uint64_t diskSize = context.SelectedDisk->Size;
    const std::uint64_t diskSectors = diskSize / SECTOR_SIZE;

    // Режим 2 всегда использует C: размером 120 GiB.
    // Режим 3 использует размер, выбранный пользователем.
    const std::uint64_t windowsSize =
        context.Mode == InstallMode::ReinstallWindowsAndFormatData
            ? 120ull * GiB
            : context.WindowsPartitionSize;
    const std::uint64_t windowsSectors = windowsSize / SECTOR_SIZE;

    if (context.Mode != InstallMode::ReinstallWindowsAndFormatData &&
        context.Mode != InstallMode::CleanDisk) {
        error = "InstallCleanDisk вызван для неподдерживаемого режима.";
        return false;
    }

    if (windowsSectors == 0) {
        error = "Размер Windows-раздела не может быть равен нулю.";
        return false;
    }

    struct ExistingPart {
        std::string device;
        std::string label;
        std::string partLabel;
        std::uint64_t number = 0;
        std::uint64_t start = 0;
        std::uint64_t sectors = 0;
    };

    // lsblk -P сохраняет пустые поля и не сдвигает столбцы.
    std::string lsblkOutput;
    std::string commandError;
    if (!RunCommandCapture("lsblk",
                           {"-b", "-P", "-p", "-o", "NAME,TYPE,LABEL,PARTLABEL,PARTN,START,SIZE", disk},
                           lsblkOutput, commandError)) {
        error = "Не удалось прочитать разметку диска. Установка остановлена.";
        return false;
    }

    const std::regex fieldRegex(R"re(([A-Z]+)="([^"]*)")re");
    std::vector<ExistingPart> existingParts;
    bool hasAmdz = false;
    ExistingPart amdz;

    std::istringstream rows(lsblkOutput);
    std::string row;
    while (std::getline(rows, row)) {
        std::map<std::string, std::string> fields;
        for (std::sregex_iterator it(row.begin(), row.end(), fieldRegex), end; it != end; ++it)
            fields[(*it)[1].str()] = (*it)[2].str();

        if (fields["TYPE"] != "part")
            continue;

        ExistingPart part;
        part.device = fields["NAME"];
        part.label = fields["LABEL"];
        part.partLabel = fields["PARTLABEL"];

        try {
            part.number = std::stoull(fields["PARTN"]);
            part.start = std::stoull(fields["START"]);
            part.sectors = std::stoull(fields["SIZE"]) / SECTOR_SIZE;
        } catch (...) {
            error = "Не удалось определить геометрию одного из разделов диска.";
            return false;
        }

        if (part.sectors == 0 || part.start > diskSectors || part.sectors > diskSectors - part.start) {
            error = "Обнаружен раздел с некорректными границами. Установка остановлена.";
            return false;
        }

        existingParts.push_back(part);

        if (ToLower(part.label) == "amdz" || ToLower(part.partLabel) == "amdz") {
            if (hasAmdz) {
                error = "Найдено несколько разделов AMDZ. Автоматическая разметка запрещена.";
                return false;
            }
            hasAmdz = true;
            amdz = part;
        }
    }

    std::uint64_t writableStart = FIRST_LBA;
    std::uint64_t writableEnd = diskSectors > GPT_BACKUP_SECTORS
                                    ? diskSectors - GPT_BACKUP_SECTORS - 1
                                    : 0;
    std::set<std::uint64_t> preservedNumbers;
    ExistingPart preservedSystem;
    bool hasPreservedSystem = false;
    bool amdZAtBeginning = false;

    if (hasAmdz) {
        if (amdz.number == 0 || amdz.sectors == 0 ||
            amdz.start + amdz.sectors > diskSectors - GPT_BACKUP_SECTORS) {
            error = "AMDZ найден, но его геометрия некорректна. Установка остановлена.";
            return false;
        }

        std::string pttypeOutput;
        if (!RunCommandCapture("lsblk", {"-dn", "-o", "PTTYPE", disk}, pttypeOutput, commandError) ||
            ToLower(pttypeOutput).find("gpt") == std::string::npos) {
            error = "AMDZ найден, но диск не размечен GPT. AMDZ не будет затронут.";
            return false;
        }

        const std::uint64_t amdzEnd = amdz.start + amdz.sectors - 1;
        const bool atBeginning = amdz.start <= AMDZ_DETECTION_RANGE_SECTORS &&
                                 amdzEnd < diskSectors / 2;
        const bool atEnd = amdzEnd >= (diskSectors > AMDZ_DETECTION_RANGE_SECTORS
                                           ? diskSectors - AMDZ_DETECTION_RANGE_SECTORS
                                           : 0) &&
                           amdz.start >= diskSectors / 2;

        if (atBeginning == atEnd) {
            error = "AMDZ находится не однозначно у начала или конца диска. "
                    "Разметка не изменена, чтобы не повредить AMDZ.";
            return false;
        }
        amdZAtBeginning = atBeginning;

        // Если рядом с AMDZ есть служебный раздел примерно 128 МиБ,
        // сохраняем его независимо от того, с какой стороны он расположен.
        // Его отсутствие допустимо: тогда сохраняем только AMDZ.
        std::uint64_t bestDistance = std::numeric_limits<std::uint64_t>::max();

        for (const auto &part : existingParts) {
            if (part.number == amdz.number ||
                part.sectors < MIN_PRESERVED_SYSTEM_SECTORS ||
                part.sectors > MAX_PRESERVED_SYSTEM_SECTORS)
                continue;

            const std::uint64_t partEnd = part.start + part.sectors - 1;
            std::uint64_t distance = std::numeric_limits<std::uint64_t>::max();
            bool candidate = false;

            if (partEnd < amdz.start) {
                distance = amdz.start - partEnd - 1;
                candidate = distance <= ADJACENCY_TOLERANCE_SECTORS;
            } else if (part.start > amdzEnd) {
                distance = part.start - amdzEnd - 1;
                candidate = distance <= ADJACENCY_TOLERANCE_SECTORS;
            }

            if (candidate && distance < bestDistance) {
                bestDistance = distance;
                preservedSystem = part;
                hasPreservedSystem = true;
            }
        }

        preservedNumbers.insert(amdz.number);
        if (hasPreservedSystem)
            preservedNumbers.insert(preservedSystem.number);

        if (atBeginning) {
            // Новые разделы размещаются после AMDZ и любого сохранённого
            // раздела, который находится сразу за AMDZ.
            writableStart = amdzEnd + 1;
            if (hasPreservedSystem && preservedSystem.start > amdzEnd) {
                writableStart = preservedSystem.start + preservedSystem.sectors;
            }
            writableStart = ((writableStart + ALIGNMENT_SECTORS - 1) / ALIGNMENT_SECTORS) * ALIGNMENT_SECTORS;
        } else {
            // На конце диска разделы Windows создаются перед AMDZ и любым
            // сохранённым разделом, который расположен непосредственно перед AMDZ.
            writableEnd = amdz.start - 1;
            if (hasPreservedSystem && preservedSystem.start < amdz.start) {
                writableEnd = preservedSystem.start - 1;
            }
            writableEnd = ((writableEnd + 1) / ALIGNMENT_SECTORS) * ALIGNMENT_SECTORS - 1;
        }

        std::string amdzMountOutput;
        if (RunCommandCapture("findmnt", {"-rn", "-S", amdz.device}, amdzMountOutput, commandError)) {
            error = "AMDZ смонтирован:\n\n" + amdzMountOutput +
                    "\n\nРазмонтируйте AMDZ перед установкой.";
            return false;
        }

    }

    if (writableEnd <= writableStart ||
        writableEnd - writableStart + 1 <= EFI_SECTORS + MSR_SECTORS + windowsSectors + MIN_DATA_SECTORS) {
        error = "Недостаточно места между защищёнными разделами для EFI, C: и D:.";
        return false;
    }

    // Проверяем каждый существующий раздел до любых изменений GPT.
    std::string mountOutput;
    if (RunCommandCapture("findmnt", {"-rn", "-S", disk}, mountOutput, commandError)) {
        error = "Целевой диск или его раздел уже смонтирован:\n\n" + mountOutput;
        return false;
    }
    for (const auto &part : existingParts) {
        // Сохранённые AMDZ и служебный раздел не изменяются, поэтому их
        // монтирование само по себе не мешает установке.
        if (preservedNumbers.count(part.number))
            continue;

        std::string partMounts;
        if (RunCommandCapture("findmnt", {"-rn", "-S", part.device}, partMounts, commandError)) {
            error = "Раздел целевого диска смонтирован:\n\n" + part.device + "\n" + partMounts +
                    "\n\nРазмонтируйте его перед установкой.";
            return false;
        }
    }

    std::string swapOutput;
    if (RunCommandCapture("swapon", {"--show=NAME", "--noheadings"}, swapOutput, commandError)) {
        std::istringstream swaps(swapOutput);
        std::string swapDevice;
        while (std::getline(swaps, swapDevice)) {
            if (!swapDevice.empty() && swapDevice.rfind(disk, 0) == 0) {
                error = "На целевом диске используется swap:\n\n" + swapDevice;
                return false;
            }
        }
    }

    std::filesystem::path executableDirectory;
    if (!GetExecutableDirectory(executableDirectory, error))
        return false;

    const std::filesystem::path bcdSys = executableDirectory / "bcd-sys-2.4-x86_64.AppImage";
    if (!std::filesystem::exists(bcdSys)) {
        error = "Не найден BCD-SYS:\n\n" + bcdSys.string();
        return false;
    }

    const std::filesystem::path isoPath = "/Windows10.iso";
    std::error_code isoError;
    if (!std::filesystem::exists(isoPath, isoError)) {
        error = isoError
            ? "Не удалось проверить ISO Windows:\n\n" + isoPath.string() + "\n\n" + isoError.message()
            : "Не найден ISO Windows:\n\n" + isoPath.string();
        return false;
    }

    // При наличии AMDZ удаляем все остальные разделы, кроме AMDZ и соседнего
    // служебного раздела около 128 МиБ. Таблица GPT целиком не пересоздаётся.
    ReportProgress(0.10f, hasAmdz ? "Сохранение AMDZ и найденного соседнего служебного раздела, очистка остальных разделов..." : "Подготовка чистой разметки...");

    if (!hasAmdz) {
        if (!RunPrivilegedCommand("sgdisk", {"--zap-all", disk}, error) ||
            !RunPrivilegedCommand("sgdisk", {"-o", disk}, error))
            return false;
        existingParts.clear();
        preservedNumbers.clear();
    } else {
        for (const auto &part : existingParts) {
            if (preservedNumbers.count(part.number))
                continue;
            if (!RunPrivilegedCommand("sgdisk", {"-d", std::to_string(part.number), disk}, error))
                return false;
        }
    }

    // Выделяем свободные номера GPT, не меняя номер AMDZ и защищённых разделов.
    std::vector<unsigned int> newNumbers;
    for (unsigned int n = 1; n <= 128 && newNumbers.size() < 4; ++n) {
        if (!preservedNumbers.count(n))
            newNumbers.push_back(n);
    }
    if (newNumbers.size() != 4) {
        error = "Не удалось выделить четыре свободных номера GPT-разделов.";
        return false;
    }

    const std::uint64_t efiStart = writableStart;
    const std::uint64_t efiEnd = efiStart + EFI_SECTORS - 1;
    const std::uint64_t msrStart = efiEnd + 1;
    const std::uint64_t msrEnd = msrStart + MSR_SECTORS - 1;
    const std::uint64_t windowsStart = msrEnd + 1;
    const std::uint64_t windowsEnd = windowsStart + windowsSectors - 1;
    const std::uint64_t dataStart = windowsEnd + 1;
    const std::uint64_t dataEnd = writableEnd;

    if (dataEnd <= dataStart || dataEnd - dataStart + 1 < MIN_DATA_SECTORS) {
        error = "После создания C: недостаточно места для D:.";
        return false;
    }

    const auto AddGptPartition = [&](unsigned int number, std::uint64_t first, std::uint64_t last,
                                     const std::string &type, const std::string &name) -> bool {
        return RunPrivilegedCommand("sgdisk",
                                    {"-n", std::to_string(number) + ":" + std::to_string(first) + ":" + std::to_string(last),
                                     "-t", std::to_string(number) + ":" + type,
                                     "-c", std::to_string(number) + ":" + name, disk},
                                    error);
    };

    if (!AddGptPartition(newNumbers[0], efiStart, efiEnd, "EF00", "EFI") ||
        !AddGptPartition(newNumbers[1], msrStart, msrEnd, "0C01", "MSR") ||
        !AddGptPartition(newNumbers[2], windowsStart, windowsEnd, "0700", "Windows") ||
        !AddGptPartition(newNumbers[3], dataStart, dataEnd, "0700", "Data"))
        return false;

    if (!RunPrivilegedCommand("partprobe", {disk}, error) ||
        !RunPrivilegedCommand("udevadm", {"settle"}, error))
        return false;
    usleep(500000);

    const auto PartitionDevice = [&](unsigned int number) {
        const bool needsP = !disk.empty() && std::isdigit(static_cast<unsigned char>(disk.back()));
        return disk + (needsP ? "p" : "") + std::to_string(number);
    };

    const std::string efiPartition = PartitionDevice(newNumbers[0]);
    const std::string windowsPartition = PartitionDevice(newNumbers[2]);
    const std::string dataPartition = PartitionDevice(newNumbers[3]);

    std::cout << "\n========================================\n"
              << " UEFI installation / " << (context.Mode == InstallMode::CleanDisk ? "manual partitioning" : "C: 120 GiB, rest D:")
              << "\nTarget: " << disk
              << "\nAMDZ: " << (hasAmdz ? amdz.device + " (preserved)" : "not found")
              << "\nPreserved system partition: " << (hasPreservedSystem ? preservedSystem.device + " (preserved)" : "none")
              << "\nAMDZ position: " << (hasAmdz ? (amdZAtBeginning ? "beginning; install after AMDZ" : "end; install before AMDZ") : "none")
              << "\nWritable start LBA: " << writableStart
              << "\nWritable end LBA: " << writableEnd
              << "\nC: " << static_cast<double>(windowsSize) / MiB << " MiB"
              << "\nD: " << static_cast<double>((dataEnd - dataStart + 1) * SECTOR_SIZE) / MiB << " MiB\n"
              << "========================================\n";

    const std::filesystem::path isoMount = "/mnt/winiso";
    const std::filesystem::path windowsMount = "/mnt/windows";
    const std::filesystem::path efiMount = "/mnt/efi";

    if (!RunPrivilegedCommand("mkdir", {"-p", isoMount.string(), windowsMount.string(), efiMount.string()}, error))
        return false;

    std::string cleanupError;
    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
    RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

    /*
     * AMDZ здесь специально НЕ форматируем.
     */
    ReportProgress(0.20f, "Форматирование EFI...");

    if (!RunPrivilegedCommand("mkfs.fat", {"-F32", "-n", "EFI", efiPartition}, error)) {
        return false;
    }

    ReportProgress(0.22f, "Форматирование Windows...");

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Windows", windowsPartition}, error)) {
        return false;
    }

    ReportProgress(0.24f, "Форматирование Data...");

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Data", dataPartition}, error)) {
        return false;
    }

    /*
     * Монтируем ISO.
     */
    ReportProgress(0.28f, "Монтирование Windows ISO...");

    if (!RunPrivilegedCommand("mount", {"-o", "loop,ro", isoPath.string(), isoMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path installWim = isoMount / "sources" / "install.wim";

    if (!std::filesystem::exists(installWim)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        error = "В ISO не найден:\n\n" + installWim.string();
        return false;
    }

    /*
     * КРИТИЧЕСКИ ВАЖНО:
     * Windows-раздел НЕ монтируем перед apply.
     * WIM накладывается непосредственно на block device.
     */
    ReportProgress(0.30f, "Установка Windows...");

    if (!RunPrivilegedCommandWithProgress(
            "wimlib-imagex", {"apply", installWim.string(), "2", windowsPartition},
            [&](float wimProgress) {
                const float globalProgress = 0.30f + wimProgress * 0.50f;
                ReportProgress(globalProgress, "Установка Windows... " + std::to_string(static_cast<int>(wimProgress * 100.0f)) + "%");
            },
            error)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("umount", {isoMount.string()}, error)) {
        return false;
    }

    /*
     * Теперь монтируем готовую Windows.
     */
    ReportProgress(0.82f, "Проверка установленной Windows...");

    if (!RunPrivilegedCommand("mount", {"-t", "ntfs3", windowsPartition, windowsMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path kernel = windowsMount / "Windows" / "System32" / "ntoskrnl.exe";

    if (!std::filesystem::exists(kernel)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Windows image была применена, но не найден:\n\n" + kernel.string();
        return false;
    }

    /*
     * Монтируем EFI и создаём Windows Boot Manager.
     */
    ReportProgress(0.84f, "Настройка UEFI-загрузчика...");

    if (!RunPrivilegedCommand("mount", {"-t", "vfat", efiPartition, efiMount.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("env",
                              {"LC_ALL=C", bcdSys.string(), windowsMount.string(), "--firmware", "UEFI", "--syspath", efiMount.string(), "--clean", "--locale", "ru-RU", "--prodname",
                               "Windows 10 Enterprise", "--verbose"},
                              error)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path bootManager = efiMount / "EFI" / "Microsoft" / "Boot" / "bootmgfw.efi";

    const std::filesystem::path bcd = efiMount / "EFI" / "Microsoft" / "Boot" / "BCD";

    if (!std::filesystem::exists(bootManager)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        error = "Не найден UEFI Windows Boot Manager:\n\n" + bootManager.string();
        return false;
    }

    if (!std::filesystem::exists(bcd)) {
        RunPrivilegedCommand("umount", {efiMount.string()}, cleanupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        error = "Не найден UEFI Windows BCD:\n\n" + bcd.string();
        return false;
    }

    if (!RunPrivilegedCommand("umount", {efiMount.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * Генерируем только config.json.
     *
     * unattend.xml и SetupComplete.cmd пока НЕ копируем:
     * текущая рабочая установка Windows проходит OOBE без них.
     */
    ReportProgress(0.94f, "Создание config.json...");

    const std::filesystem::path scriptsDirectory = executableDirectory / "assets" / "Scripts";

    const std::filesystem::path unattend = scriptsDirectory / "unattend.xml";

    const std::filesystem::path setupComplete = scriptsDirectory / "SetupComplete.cmd";

    if (!std::filesystem::exists(unattend)) {
        error = "Не найден unattend.xml:\n\n" + unattend.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!std::filesystem::exists(setupComplete)) {
        error = "Не найден SetupComplete.cmd:\n\n" + setupComplete.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path pantherDirectory = windowsMount / "Windows" / "Panther";

    const std::filesystem::path setupScriptsDirectory = windowsMount / "Windows" / "Setup" / "Scripts";

    const std::filesystem::path sysprepDirectory = windowsMount / "Windows" / "System32" / "Sysprep";

    if (!RunPrivilegedCommand("mkdir", {"-p", pantherDirectory.string(), sysprepDirectory.string(), setupScriptsDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (pantherDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (sysprepDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * SetupComplete.cmd пока НЕ копируем.
     * Код оставлен закомментированным для последующего включения.
     */
    // if (!RunPrivilegedCommand(
    //         "cp", {setupComplete.string(), (setupScriptsDirectory / "SetupComplete.cmd").string()}, error)) {
    //     RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    //     return false;
    // }

    const std::filesystem::path postInstallDirectory = windowsMount / "Programs" / "PostInstall";

    const std::filesystem::path startupDirectory = windowsMount / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";

    const std::filesystem::path psetup = executableDirectory / "assets" / "PSetup.exe";

    if (!std::filesystem::exists(psetup)) {
        error = "Не найден PSetup.exe:\n\n" + psetup.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!RunPrivilegedCommand("mkdir", {"-p", postInstallDirectory.string(), startupDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryConfig = "/tmp/PhoenixInstaller-config.json";

    if (!CreatePostInstallConfig(context, temporaryConfig, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallConfig = postInstallDirectory / "config.json";

    if (!RunPrivilegedCommand("cp", {temporaryConfig.string(), postInstallConfig.string()}, error)) {
        std::error_code removeError;
        std::filesystem::remove(temporaryConfig, removeError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(temporaryConfig, removeError);

    if (!std::filesystem::exists(postInstallConfig)) {
        error = "config.json не был скопирован:\n\n" + postInstallConfig.string();
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallPsetup = postInstallDirectory / "PSetup.exe";

    if (!RunPrivilegedCommand("cp", {psetup.string(), postInstallPsetup.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryPsetupCmd = "/tmp/PSetup.cmd";

    {
        std::ofstream stream(temporaryPsetupCmd);

        if (!stream.is_open()) {
            error = "Не удалось создать временный PSetup.cmd.";
            RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
            return false;
        }

        stream << "@echo off\r\n"
               << "start \"\" \"C:\\Programs\\PostInstall\\PSetup.exe\"\r\n";
    }

    const std::filesystem::path startupPsetupCmd = startupDirectory / "PSetup.cmd";

    if (!RunPrivilegedCommand("cp", {temporaryPsetupCmd.string(), startupPsetupCmd.string()}, error)) {
        std::error_code removePsetupError;
        std::filesystem::remove(temporaryPsetupCmd, removePsetupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removePsetupError;
    std::filesystem::remove(temporaryPsetupCmd, removePsetupError);

    ReportProgress(0.99f, "Завершение установки...");

    if (!RunPrivilegedCommand("umount", {windowsMount.string()}, error)) {
        return false;
    }

    ReportProgress(1.0f, "Установка Windows завершена.");

    std::cout << "\n========================================\n"
              << " Windows успешно установлена\n"
              << (hasAmdz ? " AMDZ сохранён\n" : " UEFI Clean Disk готов\n") << "========================================\n";

    return true;
}

bool LinuxInstallationLauncher::InstallLegacyCleanDisk(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };

    constexpr std::uint64_t SECTOR_SIZE = 512;
    constexpr std::uint64_t MiB = 1024ull * 1024ull;
    constexpr std::uint64_t FIRST_LBA = 2048;
    constexpr std::uint64_t MIN_DATA_SECTORS = 2048; // 1 MiB

    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);

    /* AMDZ относится только к UEFI/GPT и здесь намеренно не учитывается. */
    // if (disk == "/dev/sda") {
    //     error = "Операция остановлена: /dev/sda запрещён для установки Windows.";
    //     return false;
    // }

    const std::uint64_t diskSize = context.SelectedDisk->Size;
    const std::uint64_t diskSectors = diskSize / SECTOR_SIZE;
    const std::uint64_t windowsSize = context.WindowsPartitionSize;
    const std::uint64_t windowsSectors = windowsSize / SECTOR_SIZE;

    if (windowsSectors == 0) {
        error = "Размер Windows-раздела не может быть равен нулю.";
        return false;
    }

    if (diskSectors <= FIRST_LBA + windowsSectors + MIN_DATA_SECTORS) {
        error = "На диске недостаточно места для Windows и Data-раздела.";
        return false;
    }

    const std::uint64_t windowsStart = FIRST_LBA;
    const std::uint64_t windowsEnd = windowsStart + windowsSectors - 1;
    const std::uint64_t dataStart = windowsEnd + 1;
    const std::uint64_t dataSectors = diskSectors - dataStart;
    const std::uint64_t dataEnd = diskSectors - 1;

    std::cout << "\n========================================\n"
              << " Legacy BIOS / Clean Disk installation\n"
              << "========================================\n"
              << "Target:       " << disk << '\n'
              << "Disk size:    " << static_cast<double>(diskSize) / MiB << " MiB\n"
              << "Windows C:    " << static_cast<double>(windowsSize) / MiB << " MiB\n"
              << "Data D:       " << static_cast<double>(dataSectors * SECTOR_SIZE) / MiB << " MiB\n"
              << "========================================\n";

    /*
     * До любой destructive-операции убеждаемся, что диск не используется.
     */
    ReportProgress(0.07f, "Проверка целевого диска...");

    std::string mountOutput;
    std::string mountError;

    if (RunCommandCapture("findmnt", {"-rn", "-S", disk}, mountOutput, mountError)) {
        error = "Целевой диск или его раздел уже смонтирован:\n\n" + mountOutput + "\n\nПеред очисткой необходимо размонтировать его.";
        return false;
    }

    std::string swapOutput;
    std::string swapError;

    if (RunCommandCapture("swapon", {"--show=NAME", "--noheadings"}, swapOutput, swapError)) {
        std::istringstream swapStream(swapOutput);
        std::string swapDevice;

        while (std::getline(swapStream, swapDevice)) {
            if (swapDevice.empty())
                continue;

            if (swapDevice.rfind(disk, 0) == 0) {
                error = "На целевом диске используется swap:\n\n" + swapDevice + "\n\nОперация остановлена.";
                return false;
            }
        }
    }

    /*
     * Проверяем необходимые инструменты ДО wipefs.
     */
    std::filesystem::path executableDirectory;

    if (!GetExecutableDirectory(executableDirectory, error)) {
        return false;
    }

    const std::filesystem::path msSys = executableDirectory / "ms-sys";
    const std::filesystem::path bcdSys = executableDirectory / "bcd-sys-2.4-x86_64.AppImage";

    if (!std::filesystem::exists(msSys)) {
        error = "Не найден MS-SYS:\n\n" + msSys.string();
        return false;
    }

    if (!std::filesystem::exists(bcdSys)) {
        error = "Не найден BCD-SYS:\n\n" + bcdSys.string();
        return false;
    }

    const std::filesystem::path isoPath = "/Windows10.iso";
    std::error_code isoError;

    if (!std::filesystem::exists(isoPath, isoError)) {
        if (isoError) {
            error = "Не удалось проверить ISO Windows:\n\n" + isoPath.string() + "\n\nОшибка: " + isoError.message();
        }
        else {
            error = "Не найден ISO Windows:\n\n" + isoPath.string();
        }
        return false;
    }

    // AMDZ нельзя сохранять безопасно при преобразовании GPT в MBR.
    // Не начинаем разрушительную Legacy-разметку, если на диске обнаружен AMDZ.
    std::string labelOutput;
    std::string labelError;
    if (!RunCommandCapture("lsblk", {"-P", "-p", "-o", "NAME,TYPE,LABEL,PARTLABEL", disk}, labelOutput, labelError)) {
        error = "Не удалось проверить AMDZ. Legacy-очистка остановлена.";
        return false;
    }
    const std::string lowerLabels = ToLower(labelOutput);
    if (lowerLabels.find("label=\"amdz\"") != std::string::npos ||
        lowerLabels.find("partlabel=\"amdz\"") != std::string::npos) {
        error = "На диске обнаружен AMDZ. Полная установка в Legacy BIOS/MBR запрещена, "
                "поскольку она может повредить AMDZ. Загрузите установщик в режиме UEFI.";
        return false;
    }

    /*
     * Для Legacy Clean Disk создаём MBR.
     * AMDZ здесь не сохраняем и вообще не рассматриваем:
     * AMDZ является UEFI/GPT-механизмом.
     */
    ReportProgress(0.10f, "Очистка диска...");

    if (!RunPrivilegedCommand("wipefs", {"-a", disk}, error)) {
        return false;
    }

    std::ostringstream partitionTable;
    partitionTable << "label: dos\n"
                   << "unit: sectors\n"
                   << "\n"
                   << "start=" << windowsStart << ", size=" << windowsSectors << ", type=7, bootable\n"
                   << "start=" << dataStart << ", size=" << dataSectors << ", type=7\n";

    std::cout << "\nMBR partition table:\n" << partitionTable.str() << '\n';

    ReportProgress(0.14f, "Создание MBR-разметки...");

    if (!RunPrivilegedCommandWithInput("sfdisk", {disk}, partitionTable.str(), error)) {
        return false;
    }

    if (!RunPrivilegedCommand("partprobe", {disk}, error)) {
        return false;
    }

    if (!RunPrivilegedCommand("udevadm", {"settle"}, error)) {
        return false;
    }

    usleep(500000);

    /*
     * /dev/sdb  -> /dev/sdb1
     * /dev/nvme0n1 -> /dev/nvme0n1p1
     */
    const auto PartitionDevice = [&](unsigned int number) {
        const bool needsP = !disk.empty() && std::isdigit(static_cast<unsigned char>(disk.back()));
        return disk + (needsP ? "p" : "") + std::to_string(number);
    };

    const std::string windowsPartition = PartitionDevice(1);
    const std::string dataPartition = PartitionDevice(2);

    std::cout << "\nPartitions:\n"
              << "Windows: " << windowsPartition << "\n"
              << "Data:    " << dataPartition << "\n";

    ReportProgress(0.17f, "Форматирование Windows-раздела...");

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Windows", windowsPartition}, error)) {
        return false;
    }

    ReportProgress(0.20f, "Форматирование Data-раздела...");

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Data", dataPartition}, error)) {
        return false;
    }

    const std::filesystem::path isoMount = "/mnt/winiso";
    const std::filesystem::path windowsMount = "/mnt/windows";

    if (!RunPrivilegedCommand("mkdir", {"-p", isoMount.string(), windowsMount.string()}, error)) {
        return false;
    }

    std::string cleanupError;
    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

    ReportProgress(0.23f, "Монтирование Windows ISO...");

    if (!RunPrivilegedCommand("mount", {"-o", "loop,ro", isoPath.string(), isoMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path installWim = isoMount / "sources" / "install.wim";

    if (!std::filesystem::exists(installWim)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        error = "В ISO не найден:\n\n" + installWim.string();
        return false;
    }

    /* WIM применяется непосредственно к block device. NTFS заранее не монтируем. */
    ReportProgress(0.25f, "Установка Windows...");

    if (!RunPrivilegedCommandWithProgress(
            "wimlib-imagex", {"apply", installWim.string(), "2", windowsPartition},
            [&](float wimProgress) {
                const float globalProgress = 0.25f + wimProgress * 0.58f;
                ReportProgress(globalProgress, "Установка Windows... " + std::to_string(static_cast<int>(wimProgress * 100.0f)) + "%");
            },
            error)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("umount", {isoMount.string()}, error)) {
        return false;
    }

    ReportProgress(0.84f, "Проверка установленной Windows...");

    if (!RunPrivilegedCommand("mount", {"-t", "ntfs3", windowsPartition, windowsMount.string()}, error)) {
        return false;
    }

    const std::filesystem::path kernel = windowsMount / "Windows" / "System32" / "ntoskrnl.exe";

    if (!std::filesystem::exists(kernel)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        error = "Windows image была применена, но не найден:\n\n" + kernel.string();
        return false;
    }

    ReportProgress(0.87f, "Настройка Legacy BIOS-загрузчика...");

    if (!RunPrivilegedCommand(msSys.string(), {"--mbr7", disk}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand(msSys.string(), {"--ntfs", windowsPartition}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    ReportProgress(0.91f, "Создание Windows Boot Manager...");

    if (!RunPrivilegedCommand("env", {"LC_ALL=C", bcdSys.string(), windowsMount.string(), "--firmware", "BIOS", "--clean", "--locale", "ru-RU", "--prodname", "Windows 10 Enterprise", "--verbose"},
                              error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    ReportProgress(0.95f, "Проверка Legacy-загрузчика...");

    const std::filesystem::path bootManager = windowsMount / "bootmgr";
    const std::filesystem::path bcd = windowsMount / "Boot" / "BCD";

    if (!std::filesystem::exists(bootManager)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        error = "Не найден Legacy Windows Boot Manager:\n\n" + bootManager.string();
        return false;
    }

    if (!std::filesystem::exists(bcd)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        error = "Не найден Legacy Windows BCD:\n\n" + bcd.string();
        return false;
    }

    ReportProgress(0.97f, "Создание конфигурации PostInstall...");

    const std::filesystem::path scriptsDirectory = executableDirectory / "assets" / "Scripts";

    const std::filesystem::path unattend = scriptsDirectory / "unattend.xml";

    const std::filesystem::path setupComplete = scriptsDirectory / "SetupComplete.cmd";

    if (!std::filesystem::exists(unattend)) {
        error = "Не найден unattend.xml:\n\n" + unattend.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!std::filesystem::exists(setupComplete)) {
        error = "Не найден SetupComplete.cmd:\n\n" + setupComplete.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path pantherDirectory = windowsMount / "Windows" / "Panther";

    const std::filesystem::path setupScriptsDirectory = windowsMount / "Windows" / "Setup" / "Scripts";

    const std::filesystem::path sysprepDirectory = windowsMount / "Windows" / "System32" / "Sysprep";

    if (!RunPrivilegedCommand("mkdir", {"-p", pantherDirectory.string(), sysprepDirectory.string(), setupScriptsDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (pantherDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    if (!RunPrivilegedCommand("cp", {unattend.string(), (sysprepDirectory / "unattend.xml").string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    /*
     * SetupComplete.cmd пока НЕ копируем.
     * Код оставлен закомментированным для последующего включения.
     */
    // if (!RunPrivilegedCommand(
    //         "cp", {setupComplete.string(), (setupScriptsDirectory / "SetupComplete.cmd").string()}, error)) {
    //     RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
    //     return false;
    // }

    const std::filesystem::path postInstallDirectory = windowsMount / "Programs" / "PostInstall";

    const std::filesystem::path startupDirectory = windowsMount / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";

    const std::filesystem::path psetup = executableDirectory / "assets" / "PSetup.exe";

    if (!std::filesystem::exists(psetup)) {
        error = "Не найден PSetup.exe:\n\n" + psetup.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!RunPrivilegedCommand("mkdir", {"-p", postInstallDirectory.string(), startupDirectory.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryConfig = "/tmp/PhoenixInstaller-config.json";

    if (!CreatePostInstallConfig(context, temporaryConfig, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallConfig = postInstallDirectory / "config.json";

    if (!RunPrivilegedCommand("cp", {temporaryConfig.string(), postInstallConfig.string()}, error)) {
        std::error_code removeError;
        std::filesystem::remove(temporaryConfig, removeError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(temporaryConfig, removeError);

    if (!std::filesystem::exists(postInstallConfig)) {
        error = "config.json не был скопирован:\n\n" + postInstallConfig.string();
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path postInstallPsetup = postInstallDirectory / "PSetup.exe";

    if (!RunPrivilegedCommand("cp", {psetup.string(), postInstallPsetup.string()}, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    const std::filesystem::path temporaryPsetupCmd = "/tmp/PSetup.cmd";

    {
        std::ofstream stream(temporaryPsetupCmd);

        if (!stream.is_open()) {
            error = "Не удалось создать временный PSetup.cmd.";
            RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
            return false;
        }

        stream << "@echo off\r\n"
               << "start \"\" \"C:\\Programs\\PostInstall\\PSetup.exe\"\r\n";
    }

    const std::filesystem::path startupPsetupCmd = startupDirectory / "PSetup.cmd";

    if (!RunPrivilegedCommand("cp", {temporaryPsetupCmd.string(), startupPsetupCmd.string()}, error)) {
        std::error_code removePsetupError;
        std::filesystem::remove(temporaryPsetupCmd, removePsetupError);
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);
        return false;
    }

    std::error_code removePsetupError;
    std::filesystem::remove(temporaryPsetupCmd, removePsetupError);

    ReportProgress(0.99f, "Завершение установки...");

    if (!RunPrivilegedCommand("umount", {windowsMount.string()}, error)) {
        return false;
    }

    ReportProgress(1.0f, "Установка Windows завершена.");

    std::cout << "\n========================================\n"
              << " Windows успешно установлена\n"
              << " Legacy BIOS / MBR boot готов\n"
              << " Clean Disk завершён\n"
              << "========================================\n";

    return true;
}

bool LinuxInstallationLauncher::InstallLegacy(const InstallerContext &context, std::string &error, const ProgressCallback &progress) {
    auto ReportProgress = [&](float value, const std::string &status) {
        if (progress)
            progress(value, status);
    };

    if (!context.SelectedDisk) {
        error = "Диск для установки не выбран.";
        return false;
    }

    const std::string disk = NormalizeDevice(context.SelectedDisk->Device);
    ExistingPartitions partitions;

    ReportProgress(0.06f, "Поиск разделов Windows...");

    if (!FindExistingPartitions(context, partitions, error))
        return false;

    if (partitions.windows.empty()) {
        error = "Раздел Windows (C:) не найден.\n\n"
                "Невозможно безопасно выполнить переустановку Windows.";
        return false;
    }

    const bool formatData = context.Mode == InstallMode::ReinstallWindowsAndFormatData;

    if (formatData && partitions.data.empty()) {
        error = "Раздел D: не найден.\n\n"
                "Для выбранного режима установки необходим существующий "
                "раздел данных.";
        return false;
    }

    std::cout << "\n========================================\n"
              << " Legacy BIOS installation\n"
              << "========================================\n"
              << "Disk:      " << disk << '\n'
              << "Windows:   " << partitions.windows << '\n'
              << "Data:      ";

    if (partitions.data.empty())
        std::cout << "not found";
    else
        std::cout << partitions.data;

    std::cout << '\n';

    // Windows не должен быть смонтирован во время применения WIM.
    ReportProgress(0.08f, "Проверка Windows-раздела...");

    std::string mountOutput;
    std::string mountError;

    if (RunCommandCapture("findmnt", {"-rn", "-S", partitions.windows}, mountOutput, mountError)) {

        error = "Раздел Windows уже смонтирован:\n\n" + mountOutput + "\nПеред установкой его необходимо размонтировать.";
        return false;
    }

    const std::filesystem::path isoPath = "/Windows10.iso";
    const std::filesystem::path isoMount = "/mnt/winiso";
    const std::filesystem::path windowsMount = "/mnt/windows";

    std::error_code isoError;

    if (!std::filesystem::exists(isoPath, isoError)) {
        error = isoError ? "Не удалось проверить ISO Windows:\n\n" + isoPath.string() + "\n\nОшибка: " + isoError.message() : "Не найден ISO Windows:\n\n" + isoPath.string();

        return false;
    }

    if (!RunCommand("mkdir", {"-p", isoMount.string(), windowsMount.string()}, error))
        return false;

    std::string cleanupError;

    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

    RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

    // Форматирование Windows.
    ReportProgress(0.10f, "Форматирование Windows-раздела...");
    std::cout << "\nФорматирование Windows-раздела...\n";

    if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Windows", partitions.windows}, error))
        return false;

    if (formatData) {
        ReportProgress(0.12f, "Форматирование Data-раздела...");
        std::cout << "\nФорматирование Data-раздела...\n";

        if (!RunPrivilegedCommand("mkfs.ntfs", {"-f", "-L", "Data", partitions.data}, error))
            return false;
    }

    // Монтирование ISO.
    ReportProgress(0.15f, "Монтирование Windows ISO...");
    std::cout << "\nМонтирование Windows ISO...\n";

    if (!RunPrivilegedCommand("mount", {"-o", "loop,ro", isoPath.string(), isoMount.string()}, error))
        return false;

    const std::filesystem::path installWim = isoMount / "sources" / "install.wim";

    if (!std::filesystem::exists(installWim)) {
        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

        error = "В ISO не найден:\n\n" + installWim.string();
        return false;
    }

    // Применяем WIM напрямую к Windows-разделу.
    ReportProgress(0.20f, "Применение Windows...");
    std::cout << "\nПрименение Windows image (index 2)...\n";

    if (!RunPrivilegedCommandWithProgress(
            "wimlib-imagex", {"apply", installWim.string(), "2", partitions.windows},
            [&](float wimProgress) {
                const float globalProgress = 0.20f + wimProgress * 0.60f;

                ReportProgress(globalProgress, "Установка Windows... " + std::to_string(static_cast<int>(wimProgress * 100.0f)) + "%");
            },
            error)) {

        RunPrivilegedCommand("umount", {isoMount.string()}, cleanupError);

        return false;
    }

    ReportProgress(0.80f, "Windows image применена.");

    if (!RunPrivilegedCommand("umount", {isoMount.string()}, error))
        return false;

    // Монтируем установленную Windows.
    ReportProgress(0.83f, "Проверка установленной Windows...");

    if (!RunPrivilegedCommand("mount", {"-t", "ntfs3", partitions.windows, windowsMount.string()}, error))
        return false;

    const std::filesystem::path kernel = windowsMount / "Windows" / "System32" / "ntoskrnl.exe";

    if (!std::filesystem::exists(kernel)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Windows image была применена, но не найден:\n\n" + kernel.string();

        return false;
    }

    std::cout << "\nWindows image успешно применена.\n";

    // Пути к инструментам установщика.
    std::filesystem::path executableDirectory;

    if (!GetExecutableDirectory(executableDirectory, error)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path msSys = executableDirectory / "ms-sys";

    const std::filesystem::path bcdSys = executableDirectory / "bcd-sys-2.4-x86_64.AppImage";

    if (!std::filesystem::exists(msSys)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден MS-SYS:\n\n" + msSys.string();
        return false;
    }

    if (!std::filesystem::exists(bcdSys)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден BCD-SYS:\n\n" + bcdSys.string();
        return false;
    }

    // Legacy BIOS boot.
    ReportProgress(0.87f, "Настройка Legacy BIOS-загрузчика...");

    std::cout << "\nЗапись Windows MBR...\n";

    if (!RunPrivilegedCommand(msSys.string(), {"--mbr7", disk}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    std::cout << "\nЗапись NTFS boot sector...\n";

    if (!RunPrivilegedCommand(msSys.string(), {"--ntfs", partitions.windows}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    ReportProgress(0.91f, "Создание Windows Boot Manager...");

    std::cout << "\nНастройка Windows Boot Manager...\n";

    if (!RunPrivilegedCommand("env", {"LC_ALL=C", bcdSys.string(), windowsMount.string(), "--firmware", "BIOS", "--clean", "--locale", "ru-RU", "--prodname", "Windows 10 Enterprise", "--verbose"},
                              error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    // Проверка загрузчика.
    ReportProgress(0.96f, "Проверка Legacy-загрузчика...");

    const std::filesystem::path bootManager = windowsMount / "bootmgr";

    const std::filesystem::path bcd = windowsMount / "Boot" / "BCD";

    if (!std::filesystem::exists(bootManager)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден Legacy Windows Boot Manager:\n\n" + bootManager.string();

        return false;
    }

    if (!std::filesystem::exists(bcd)) {
        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        error = "Не найден Legacy Windows BCD:\n\n" + bcd.string();

        return false;
    }

    std::cout << "\nПроверка Legacy-загрузчика прошла успешно.\n";

    // Файлы первого запуска.
    ReportProgress(0.97f, "Установка файлов первого запуска...");

    const std::filesystem::path scriptsDirectory = executableDirectory / "assets" / "Scripts";

    const std::filesystem::path unattend = scriptsDirectory / "unattend.xml";

    const std::filesystem::path setupComplete = scriptsDirectory / "SetupComplete.cmd";

    const std::filesystem::path pantherDirectory = windowsMount / "Windows" / "Panther";

    const std::filesystem::path setupScriptsDirectory = windowsMount / "Windows" / "Setup" / "Scripts";

    const std::filesystem::path sysprepDirectory = windowsMount / "Windows" / "System32" / "Sysprep";

    if (!std::filesystem::exists(unattend)) {
        error = "Не найден unattend.xml:\n\n" + unattend.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!std::filesystem::exists(setupComplete)) {
        error = "Не найден SetupComplete.cmd:\n\n" + setupComplete.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!RunPrivilegedCommand("mkdir", {"-p", pantherDirectory.string(), sysprepDirectory.string(), setupScriptsDirectory.string()}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    // unattend.xml -> Panther
    if (!RunPrivilegedCommand("cp", {unattend.string(), (pantherDirectory / "unattend.xml").string()}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    // unattend.xml -> Sysprep
    if (!RunPrivilegedCommand("cp", {unattend.string(), (sysprepDirectory / "unattend.xml").string()}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    /*
     * SetupComplete.cmd пока НЕ копируем.
     * Код оставлен для последующего включения.
     */

    // if (!RunPrivilegedCommand("cp", {setupComplete.string(), (setupScriptsDirectory / "SetupComplete.cmd").string()}, error)) {

    //    RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

    //    return false;
    //}

    // PostInstall.
    const std::filesystem::path postInstallDirectory = windowsMount / "Programs" / "PostInstall";

    const std::filesystem::path startupDirectory = windowsMount / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";

    const std::filesystem::path psetup = executableDirectory / "assets" / "PSetup.exe";

    if (!std::filesystem::exists(psetup)) {
        error = "Не найден PSetup.exe:\n\n" + psetup.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    if (!RunPrivilegedCommand("mkdir", {"-p", postInstallDirectory.string(), startupDirectory.string()}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path temporaryConfig = "/tmp/PhoenixInstaller-config.json";

    if (!CreatePostInstallConfig(context, temporaryConfig, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path postInstallConfig = postInstallDirectory / "config.json";

    if (!RunPrivilegedCommand("cp", {temporaryConfig.string(), postInstallConfig.string()}, error)) {

        std::error_code removeError;
        std::filesystem::remove(temporaryConfig, removeError);

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(temporaryConfig, removeError);

    if (!std::filesystem::exists(postInstallConfig)) {
        error = "config.json не был скопирован:\n\n" + postInstallConfig.string();

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    const std::filesystem::path postInstallPsetup = postInstallDirectory / "PSetup.exe";

    if (!RunPrivilegedCommand("cp", {psetup.string(), postInstallPsetup.string()}, error)) {

        RunPrivilegedCommand("umount", {windowsMount.string()}, cleanupError);

        return false;
    }

    ReportProgress(0.99f, "Завершение установки...");

    if (!RunPrivilegedCommand("umount", {windowsMount.string()}, error))
        return false;

    ReportProgress(1.0f, "Установка Windows завершена.");

    std::cout << "\n========================================\n"
              << " Windows успешно установлена\n"
              << " Legacy BIOS boot готов\n"
              << "========================================\n";

    return true;
}