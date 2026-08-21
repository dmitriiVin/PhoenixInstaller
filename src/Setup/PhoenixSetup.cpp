#include "PhoenixSetup.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Packages/PackageManager.h"

namespace {

class Logger {
  public:
    explicit Logger(const std::filesystem::path &file) : m_file(file) {
        std::error_code error;
        std::filesystem::create_directories(m_file.parent_path(), error);
    }

    void Write(const char *level, const std::string &message) const {
        std::ofstream output(m_file, std::ios::binary | std::ios::app);

        if (output.is_open())
            output << '[' << level << "] " << message << '\n';
    }

  private:
    std::filesystem::path m_file;
};

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

bool RunScript(const std::filesystem::path &script, const std::vector<std::wstring> &arguments, const std::filesystem::path &workingDirectory,
               const Logger &logger) {
    if (!std::filesystem::is_regular_file(script)) {
        logger.Write("ERROR", "Required script is missing: " + script.string());
        return false;
    }

    std::wstring command = L"cmd.exe /d /c call " + QuoteArgument(script.wstring());

    for (const auto &argument : arguments) {
        command.push_back(L' ');
        command += QuoteArgument(argument);
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startupInfo,
                        &processInfo)) {
        logger.Write("ERROR", "Unable to start script: " + script.string());
        return false;
    }

    CloseHandle(processInfo.hThread);
    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);

    if (exitCode != 0) {
        logger.Write("ERROR", "Script failed with ErrorLevel=" + std::to_string(exitCode) + ": " + script.string());
        return false;
    }

    return true;
}

bool ReadConfiguration(const std::filesystem::path &configPath, nlohmann::json &configuration, const Logger &logger) {
    std::ifstream file(configPath);

    if (!file.is_open()) {
        logger.Write("ERROR", "Unable to open config.json.");
        return false;
    }

    try {
        file >> configuration;
    }
    catch (const nlohmann::json::exception &exception) {
        logger.Write("ERROR", "Unable to parse config.json: " + std::string(exception.what()));
        return false;
    }

    if (!configuration.is_object()) {
        logger.Write("ERROR", "config.json must contain an object.");
        return false;
    }

    return true;
}

bool InstallPackages(const nlohmann::json &configuration, const std::filesystem::path &root, const Logger &logger) {
    if (!configuration.contains("packages") || !configuration["packages"].is_array()) {
        logger.Write("ERROR", "config.json does not contain a valid packages array.");
        return false;
    }

    PackageManager packages;

    if (!packages.Load((root / L"packages.json").string())) {
        logger.Write("ERROR", "Unable to load the staged packages.json catalog.");
        return false;
    }

    for (const auto &item : configuration["packages"]) {
        if (!item.is_string()) {
            logger.Write("ERROR", "Package ids in config.json must be strings.");
            return false;
        }

        const std::string packageId = item.get<std::string>();
        const Package *package = packages.FindById(packageId);

        if (!package) {
            logger.Write("ERROR", "Selected package is absent from packages.json: " + packageId);
            return false;
        }

        const auto installer = root / L"Programs" / package->Folder / package->Installer;

        if (!RunScript(root / L"Scripts" / L"Programs" / L"InstallPrograms.bat",
                       {std::wstring(packageId.begin(), packageId.end()), installer.wstring(), std::wstring(package->Arguments.begin(), package->Arguments.end())},
                       root, logger)) {
            return false;
        }
    }

    return true;
}

} // namespace

int PhoenixSetup::Run(const std::filesystem::path &configPath) {
    const auto root = configPath.parent_path();
    Logger logger(root / L"Logs" / L"setup.log");
    logger.Write("INFO", "Starting PhoenixSetup.");

    nlohmann::json configuration;

    if (!ReadConfiguration(configPath, configuration, logger))
        return 1;

    const auto scripts = root / L"Scripts";

    if (!RunScript(scripts / L"Windows" / L"ConfigureWindows.bat", {}, root, logger))
        return 1;

    bool installDrivers = true;

    try {
        if (configuration.contains("drivers"))
            installDrivers = configuration.at("drivers").value("install", true);
    }
    catch (const nlohmann::json::exception &) {
        logger.Write("ERROR", "drivers.install must be a Boolean value.");
        return 1;
    }

    if (installDrivers) {
        if (!RunScript(scripts / L"Drivers" / L"InstallDrivers.bat", {(root / L"Drivers").wstring()}, root, logger))
            return 1;
    }
    else {
        logger.Write("INFO", "Driver installation was disabled by config.json.");
    }

    if (!InstallPackages(configuration, root, logger))
        return 1;

    if (!RunScript(scripts / L"Tweaks" / L"WindowsTweaks.bat", {}, root, logger))
        return 1;

    logger.Write("INFO", "PhoenixSetup completed successfully.");
    return 0;
}
