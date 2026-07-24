#pragma once

#include <windows.h>

class Window {
  public:
    bool Create(const wchar_t *title, int width, int height, WNDPROC proc);

    bool ProcessMessages();

    HWND GetHandle() const;

    void Destroy();

  private:
    HWND m_Window = nullptr;

    HINSTANCE m_Instance = nullptr;
};
