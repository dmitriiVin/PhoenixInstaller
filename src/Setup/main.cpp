#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string_view>

#include "PhoenixSetup.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argumentCount = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    std::filesystem::path configPath = L"C:\\Phoenix\\config.json";

    if (arguments) {
        for (int index = 1; index + 1 < argumentCount; ++index) {
            if (std::wstring_view(arguments[index]) == L"--config") {
                configPath = arguments[index + 1];
                ++index;
            }
        }

        LocalFree(arguments);
    }

    return PhoenixSetup::Run(configPath);
}
