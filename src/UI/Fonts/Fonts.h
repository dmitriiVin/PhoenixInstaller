#pragma once

struct ImFont;

namespace Fonts {
bool Load();

ImFont *Small();
ImFont *Body();
ImFont *Heading();
ImFont *Title();
} // namespace Fonts