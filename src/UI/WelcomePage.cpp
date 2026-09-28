#include "WelcomePage.h"

#include "Resources/TextureManager.h"
#include "UI/Fonts/Fonts.h"
#include "UI/Fonts/IconsFontAwesome6.h"
#include "UI/Fonts/IconsFontAwesome6Brands.h"
#include "UI/Layout/Layout.h"
#include "UI/Widgets/Widgets.h"

#include <cstdlib>
#include <imgui.h>

void WelcomePage::Draw() {
    Layout::Begin();
    Layout::BeginContent();
    Layout::BeginContainer(900.0f);

    // Верхний отступ
    Layout::Space(40);

    // Главный заголовок
    Texture *logo = TextureManager::Get("assets/images/logo.png");

    if (logo && logo->IsValid() && logo->Height() > 0) {
        float logoHeight = Layout::Scale(128.0f);
        float logoWidth = logoHeight * (static_cast<float>(logo->Width()) / static_cast<float>(logo->Height()));

        ImGui::Image(static_cast<ImTextureID>(logo->Get()), ImVec2(logoWidth, logoHeight));

        ImGui::SameLine(0.0f, Layout::Scale(20.0f));

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (logoHeight - ImGui::GetTextLineHeight()) * 0.5f);
    }

    ImGui::PushFont(Fonts::Title());
    ImGui::TextUnformatted("Установщик Windows");
    ImGui::PopFont();

    Layout::Space(20);

    ImGui::Separator();

    Layout::Space(42);

    // Заголовок страницы
    ImGui::PushFont(Fonts::Heading());
    ImGui::TextUnformatted("Добро пожаловать!");
    ImGui::PopFont();

    Layout::Space(20);

    ImGui::PushFont(Fonts::Body());
    ImGui::TextUnformatted("Будут выполнены следующие действия:");
    ImGui::PopFont();

    Layout::Space(16);

    UI::Bullet("Установка Windows");
    UI::Bullet("Установка драйверов");
    UI::Bullet("Установка программ");
    UI::Bullet("Настройка системы после установки");

    Layout::Space(60);

    if (ImGui::Button(ICON_FA_WINDOWS "  Установить Windows", Layout::Scale(350.0f, 72.0f))) {
        m_NextRequested = true;
    }

    ImGui::SameLine(0.0f, Layout::Scale(18.0f));

    if (ImGui::Button(ICON_FA_POWER_OFF "  Выход", Layout::Scale(180.0f, 72.0f))) {
        std::exit(0);
    }

    Layout::EndContainer();
    Layout::EndContent();
    Layout::End();
}

bool WelcomePage::NextRequested() const {
    return m_NextRequested;
}

void WelcomePage::ResetState() {
    m_NextRequested = false;
}