#pragma once

#include <string>

inline std::string WideToUtf8(const std::wstring &str) {
    return std::string(str.begin(), str.end());
}

inline std::wstring Utf8ToWide(const std::string &str) {
    return std::wstring(str.begin(), str.end());
}