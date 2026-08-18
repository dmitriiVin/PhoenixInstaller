#include "ProgramCard.h"

#include "Card.h"

#include "UI/Fonts/IconsFontAwesome6.h"

#include <imgui.h>

bool ProgramCard::Draw(const Package &package, bool selected) {
    ImGui::PushID(package.Id.c_str());

    Card::Begin("ProgramCard", selected);

    ImGui::Text("%s  %s", ICON_FA_DOWNLOAD, package.Name.c_str());

    ImGui::Separator();

    if (!package.Description.empty()) {
        ImGui::TextDisabled("%s", package.Description.c_str());
    }
    else {
        ImGui::TextDisabled("Описание отсутствует");
    }

    bool clicked = Card::End();

    ImGui::PopID();

    return clicked;
}