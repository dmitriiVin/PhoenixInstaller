#include "Fonts.h"

#include "IconsFontAwesome6.h"
#include "IconsFontAwesome6Brands.h"

#include "Utils/ResourceManager.h"

#include "imgui.h"

#include <cstdint>
#include <vector>

namespace {

std::vector<std::uint8_t> g_FontData;
std::vector<std::uint8_t> g_SolidIconsData;
std::vector<std::uint8_t> g_BrandsIconsData;

ImFont *g_Small = nullptr;
ImFont *g_Body = nullptr;
ImFont *g_Heading = nullptr;
ImFont *g_Title = nullptr;

constexpr ImWchar SolidRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

constexpr ImWchar BrandRanges[] = {ICON_MIN_FAB, ICON_MAX_FAB, 0};

ImFont *LoadFont(ImGuiIO &io, float size) {
    ImFontConfig textCfg{};
    textCfg.FontDataOwnedByAtlas = false;
    textCfg.OversampleH = 2;
    textCfg.OversampleV = 2;

    ImFont *font = io.Fonts->AddFontFromMemoryTTF(g_FontData.data(), static_cast<int>(g_FontData.size()), size, &textCfg, io.Fonts->GetGlyphRangesCyrillic());

    if (!font)
        return nullptr;

    ImFontConfig solidCfg{};
    solidCfg.MergeMode = true;
    solidCfg.PixelSnapH = true;
    solidCfg.FontDataOwnedByAtlas = false;

    io.Fonts->AddFontFromMemoryTTF(g_SolidIconsData.data(), static_cast<int>(g_SolidIconsData.size()), size, &solidCfg, SolidRanges);

    ImFontConfig brandCfg{};
    brandCfg.MergeMode = true;
    brandCfg.PixelSnapH = true;
    brandCfg.FontDataOwnedByAtlas = false;

    io.Fonts->AddFontFromMemoryTTF(g_BrandsIconsData.data(), static_cast<int>(g_BrandsIconsData.size()), size, &brandCfg, BrandRanges);

    return font;
}

} // namespace

bool Fonts::Load() {
    ImGuiIO &io = ImGui::GetIO();

    g_FontData = ResourceManager::Load("assets/fonts/Inter-Regular.otf");

    g_SolidIconsData = ResourceManager::Load("assets/fonts/fa-solid-900.ttf");

    g_BrandsIconsData = ResourceManager::Load("assets/fonts/fa-brands-400.otf");

    if (g_FontData.empty() || g_SolidIconsData.empty() || g_BrandsIconsData.empty()) {
        return false;
    }

    g_Small = LoadFont(io, 18.0f);
    g_Body = LoadFont(io, 22.0f);
    g_Heading = LoadFont(io, 30.0f);
    g_Title = LoadFont(io, 40.0f);

    if (!g_Small || !g_Body || !g_Heading || !g_Title) {
        return false;
    }

    io.FontDefault = g_Body;

    return true;
}

ImFont *Fonts::Small() {
    return g_Small;
}

ImFont *Fonts::Body() {
    return g_Body;
}

ImFont *Fonts::Heading() {
    return g_Heading;
}

ImFont *Fonts::Title() {
    return g_Title;
}