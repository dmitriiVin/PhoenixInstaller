#pragma once

#include <Windows.h>
#include <string>

inline std::string WideToUtf8(const std::wstring &str) {
    if (str.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);

    std::string result(size, '\0');

    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size, nullptr, nullptr);

    return result;
}

inline std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty())
        return {};

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);

    if (size <= 0)
        return {};

    std::wstring result(size, L'\0');

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), result.data(), size);

    return result;
}