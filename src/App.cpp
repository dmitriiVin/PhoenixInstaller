#include "App.h"

#include "Installer/LinuxInstallationLauncher.h"
#include "Resources/TextureManager.h"
#include "UI/Fonts/Fonts.h"
#include "UI/Theme/Theme.h"
#include "Utils/ResourceManager.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <iostream>
#include <sstream>
#include <string>

App::App() : m_InstallPage(m_Context),
             m_InstallModePage(m_Context),
             m_WindowsPartitionPage(m_Context),
             m_ProgramPage(m_Context),
             m_ComputerSettingsPage(m_Context),
             m_ConfirmPage(m_Context) {
}

bool App::Initialize() {
    // =========================
    // WINDOW
    // =========================

    if (!m_Window.Create("Phoenix Installer", 1280, 720)) {
        std::cerr << "Failed to create window.\n";
        return false;
    }

    // =========================
    // RENDERER
    // =========================

    if (!m_Renderer.Initialize(m_Window.GetHandle())) {
        std::cerr << "Failed to initialize renderer.\n";
        return false;
    }

    // =========================
    // IMGUI
    // =========================

    if (!m_ImGui.Initialize(m_Window.GetHandle())) {
        std::cerr << "Failed to initialize ImGui.\n";
        return false;
    }

    // =========================
    // FONTS
    // =========================

    if (!Fonts::Load()) {
        std::cerr << "Failed to load fonts.\n";
        return false;
    }

    // =========================
    // TEXTURES
    // =========================

    if (!TextureManager::Initialize()) {
        std::cerr << "Failed to initialize texture manager.\n";
        return false;
    }

    // =========================
    // THEME
    // =========================

    Theme::Apply();

    // =========================
    // PACKAGES
    // =========================

    auto data = ResourceManager::Load("packages.json");

    if (data.empty()) {
        std::cerr << "Failed to load packages resource.\n";
        return false;
    }

    std::string json(data.begin(), data.end());

    std::istringstream stream(json);

    if (!m_Context.Packages.Load(stream)) {
        std::cerr << "Failed to parse packages.json.\n";
        return false;
    }

    // =========================
    // START PAGE
    // =========================

    m_PageManager.SetPage(&m_WelcomePage);

    return true;
}

void App::Run() {
    while (m_Window.ProcessMessages()) {
        // =========================
        // FRAME BEGIN
        // =========================

        m_Renderer.BeginFrame();

        m_ImGui.BeginFrame();

        // =========================
        // DRAW CURRENT PAGE
        // =========================

        m_PageManager.Draw();

        // =========================
        // FORWARD
        // =========================

        if (m_WelcomePage.NextRequested()) {
            m_WelcomePage.ResetState();
            m_PageManager.SetPage(&m_InstallPage);
        }

        if (m_InstallPage.NextRequested()) {
            m_InstallPage.ResetState();
            m_PageManager.SetPage(&m_InstallModePage);
        }

        if (m_InstallModePage.NextRequested()) {
            m_InstallModePage.ResetState();

            if (m_Context.Mode == InstallMode::CleanDisk) {
                m_PageManager.SetPage(&m_WindowsPartitionPage);
            }
            else {
                m_PageManager.SetPage(&m_ProgramPage);
            }
        }

        if (m_WindowsPartitionPage.NextRequested()) {
            m_WindowsPartitionPage.ResetState();
            m_PageManager.SetPage(&m_ProgramPage);
        }

        if (m_ProgramPage.NextRequested()) {
            m_ProgramPage.ResetState();
            m_PageManager.SetPage(&m_ComputerSettingsPage);
        }

        if (m_ComputerSettingsPage.NextRequested()) {
            m_ComputerSettingsPage.ResetState();
            m_PageManager.SetPage(&m_ConfirmPage);
        }

        // =========================
        // START INSTALLATION
        // =========================

        if (m_ConfirmPage.NextRequested()) {
            m_ConfirmPage.ResetState();

            // На всякий случай дожидаемся
            // предыдущего потока.
            if (m_InstallationThread.joinable()) {
                m_InstallationThread.join();
            }

            // Переводим InstallPage
            // в режим отображения прогресса.
            m_InstallPage.StartInstallation();

            // Сразу показываем страницу установки.
            m_PageManager.SetPage(&m_InstallPage);

            // Запускаем установку в отдельном потоке,
            // чтобы GUI не зависал.
            m_InstallationThread = std::thread([this]() {
                std::string error;

                const bool success = LinuxInstallationLauncher::Install(m_Context, error, [this](float progress, const std::string &status) { m_InstallPage.SetProgress(progress, status); });

                if (success) {
                    m_InstallPage.SetFinished();
                }
                else {
                    std::cerr << "Installation failed: " << error << '\n';

                    m_InstallPage.SetFailed(error);
                }
            });
        }

        // =========================
        // BACK
        // =========================

        // Назад с InstallPage разрешаем
        // только если установка сейчас не идёт.
        if (m_InstallPage.BackRequested() && !m_InstallPage.IsInstalling()) {
            m_InstallPage.ResetState();
            m_PageManager.SetPage(&m_WelcomePage);
        }

        if (m_InstallModePage.BackRequested()) {
            m_InstallModePage.ResetState();
            m_PageManager.SetPage(&m_InstallPage);
        }

        if (m_WindowsPartitionPage.BackRequested()) {
            m_WindowsPartitionPage.ResetState();
            m_PageManager.SetPage(&m_InstallModePage);
        }

        if (m_ProgramPage.BackRequested()) {
            m_ProgramPage.ResetState();

            if (m_Context.Mode == InstallMode::CleanDisk) {
                m_PageManager.SetPage(&m_WindowsPartitionPage);
            }
            else {
                m_PageManager.SetPage(&m_InstallModePage);
            }
        }

        if (m_ComputerSettingsPage.BackRequested()) {
            m_ComputerSettingsPage.ResetState();
            m_PageManager.SetPage(&m_ProgramPage);
        }

        if (m_ConfirmPage.BackRequested()) {
            m_ConfirmPage.ResetState();
            m_PageManager.SetPage(&m_ProgramPage);
        }

        // =========================
        // FRAME END
        // =========================

        m_ImGui.EndFrame();

        m_Renderer.EndFrame();
    }
}

void App::Shutdown() {
    // Если установка ещё выполняется,
    // обязательно дождаться worker thread.
    if (m_InstallationThread.joinable()) {
        m_InstallationThread.join();
    }

    TextureManager::Shutdown();

    m_ImGui.Shutdown();

    m_Renderer.Shutdown();

    m_Window.Destroy();
}