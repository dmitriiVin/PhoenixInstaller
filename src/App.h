#pragma once

#include <vector>

#include "Core/InstallerContext.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"
#include "UI/ImGuiManager.h"
#include "UI/InstallModePage.h"
#include "UI/InstallPage.h"
#include "UI/PageManager.h"
#include "UI/ProgramPage.h"
#include "UI/WelcomePage.h"
#include "UI/WindowsPartitionPage.h"

class App {
  public:
    App();
    bool Initialize();
    void Run();
    void Shutdown();

  private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static Renderer *s_Renderer;

    std::vector<unsigned char> m_FontData;

    Window m_Window;
    Renderer m_Renderer;
    ImGuiManager m_ImGui;

    PageManager m_PageManager;

    InstallerContext m_Context;

    WelcomePage m_WelcomePage;
    InstallPage m_InstallPage;
    InstallModePage m_InstallModePage;
    WindowsPartitionPage m_WindowsPartitionPage;
    ProgramPage m_ProgramPage;
};