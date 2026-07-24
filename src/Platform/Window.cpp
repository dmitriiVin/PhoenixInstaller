#include "Window.h"

bool Window::Create(const wchar_t *title, int width, int height, WNDPROC proc) {
    m_Instance = GetModuleHandle(nullptr);

    WNDCLASSEX wc{};

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = proc;
    wc.hInstance = m_Instance;
    wc.lpszClassName = L"PhoenixInstaller";

    RegisterClassEx(&wc);

    m_Window = CreateWindowEx(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, m_Instance, nullptr);

    if (!m_Window)
        return false;

    ShowWindow(m_Window, SW_SHOW);
    UpdateWindow(m_Window);

    return true;
}

bool Window::ProcessMessages() {
    MSG msg{};

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

HWND Window::GetHandle() const {
    return m_Window;
}

void Window::Destroy() {
    if (m_Window) {
        DestroyWindow(m_Window);
        m_Window = nullptr;
    }
}
