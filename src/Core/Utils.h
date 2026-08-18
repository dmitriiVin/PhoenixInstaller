#pragma once

#include <Windows.h>
#include <string>

inline std::string WideToUtf8(const std::wstring &str) {
    if (str.empty())
        return {};

    int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string result(size - 1, '\0');

    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, result.data(), size, nullptr, nullptr);

    return result;
}