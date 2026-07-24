#pragma once

#include "imgui.h"

namespace Theme {
void Apply();

inline constexpr ImVec4 Background = ImVec4(21 / 255.0f, 24 / 255.0f, 35 / 255.0f, 1.0f);

inline constexpr ImVec4 Accent = ImVec4(59 / 255.0f, 130 / 255.0f, 246 / 255.0f, 1.0f);
inline constexpr ImVec4 AccentHovered = ImVec4(79 / 255.0f, 150 / 255.0f, 255 / 255.0f, 1.0f);
inline constexpr ImVec4 AccentPressed = ImVec4(42 / 255.0f, 112 / 255.0f, 222 / 255.0f, 1.0f);

inline constexpr ImVec4 Text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
inline constexpr ImVec4 SecondaryText = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
} // namespace Theme