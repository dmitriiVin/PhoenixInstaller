#include "App.h"

#include <windows.h>

#include "Platform/Window.h"
#include "Resources/TextureManager.h"
#include "UI/Fonts/Fonts.h"
#include "UI/ImGuiManager.h"
#include "UI/InstallPage.h"
#include "UI/Theme/Theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

Renderer *App::s_Renderer = nullptr;

App::App() : m_InstallPage(m_Context) {
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
    m_PageManager.SetPage(&m_WelcomePage);
    return true;
}

void App::Run() {
    while (m_Window.ProcessMessages()) {
        m_Renderer.BeginFrame();
        m_ImGui.BeginFrame();

        m_PageManager.Draw();

        if (m_WelcomePage.NextRequested()) {
            m_WelcomePage.ResetState();
            m_PageManager.SetPage(&m_InstallPage);
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