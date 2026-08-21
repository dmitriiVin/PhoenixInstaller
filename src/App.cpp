#include "App.h"

#include <windows.h>

#include "Platform/Window.h"
#include "Installer/InstallationLauncher.h"
#include "Resources/TextureManager.h"
#include "UI/Fonts/Fonts.h"
#include "UI/ImGuiManager.h"
#include "UI/InstallPage.h"
#include "UI/Theme/Theme.h"
#include "Utils/ResourceManager.h"
#include "resource.h"

#include <filesystem>
#include <sstream>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

Renderer *App::s_Renderer = nullptr;

App::App() : m_InstallPage(m_Context), m_InstallModePage(m_Context), m_WindowsPartitionPage(m_Context), m_ProgramPage(m_Context), m_ConfirmPage(m_Context) {
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return TRUE;
    switch (msg) {
    case WM_SIZE: {
        if (s_Renderer && wParam != SIZE_MINIMIZED) {
            s_Renderer->Resize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool App::Initialize() {
    if (!m_Window.Create(L"Windows Installer", 1280, 720, WindowProc))
        return false;

    if (!m_Renderer.Initialize(m_Window.GetHandle()))
        return false;

    s_Renderer = &m_Renderer;

    if (!m_ImGui.Initialize(m_Window.GetHandle(), m_Renderer.GetDevice(), m_Renderer.GetContext())) {
        return false;
    }

    if (!Fonts::Load()) {
        MessageBoxA(nullptr, "Failed to load fonts.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!TextureManager::Initialize(m_Renderer.GetDevice())) {
        MessageBoxA(nullptr, "Failed to initialize texture manager.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    Theme::Apply();

    auto data = ResourceManager::Load(IDR_PACKAGES_JSON);

    if (data.empty()) {
        MessageBoxA(nullptr, "Failed to load packages resource.", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    std::string json(data.begin(), data.end());
    std::istringstream stream(json);

    if (!m_Context.Packages.Load(stream)) {
        MessageBoxA(nullptr, "Failed to parse packages.json", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    m_PageManager.SetPage(&m_WelcomePage);

    return true;
}

void App::Run() {
    while (m_Window.ProcessMessages()) {
        m_Renderer.BeginFrame();
        m_ImGui.BeginFrame();

        m_PageManager.Draw();

        //======================= ВПЕРЕД =======================\\

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

            if (m_Context.InstallMode == InstallMode::CleanDisk)
                m_PageManager.SetPage(&m_WindowsPartitionPage);
            else
                m_PageManager.SetPage(&m_ProgramPage);
        }

        if (m_WindowsPartitionPage.NextRequested()) {
            m_WindowsPartitionPage.ResetState();
            m_PageManager.SetPage(&m_ProgramPage);
        }

        if (m_ProgramPage.NextRequested()) {
            m_ProgramPage.ResetState();
            m_PageManager.SetPage(&m_ConfirmPage);
        }

        if (m_ConfirmPage.NextRequested()) {
            m_ConfirmPage.ResetState();

            std::wstring error;

            if (!InstallationLauncher::Launch(m_Context, error)) {
                MessageBoxW(m_Window.GetHandle(), error.c_str(), L"Phoenix Installer", MB_OK | MB_ICONERROR);
            }
            else {
                PostQuitMessage(0);
            }
        }

        //======================= НАЗАД =======================\\

        if (m_InstallPage.BackRequested()) {
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

            if (m_Context.InstallMode == InstallMode::CleanDisk)
                m_PageManager.SetPage(&m_WindowsPartitionPage);
            else
                m_PageManager.SetPage(&m_InstallModePage);
        }

        if (m_ConfirmPage.BackRequested()) {
            m_ConfirmPage.ResetState();
            m_PageManager.SetPage(&m_ProgramPage);
        }

        m_ImGui.EndFrame();
        m_Renderer.EndFrame();
    }
}

void App::Shutdown() {
    s_Renderer = nullptr;

    TextureManager::Shutdown();

    m_ImGui.Shutdown();
    m_Renderer.Shutdown();
    m_Window.Destroy();
}
