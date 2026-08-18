#pragma once

#include <d3d11.h>
#include <windows.h>

class Renderer {
  public:
    bool Initialize(HWND hwnd);

    void BeginFrame();
    void EndFrame();

    void Resize(UINT width, UINT height);

    void Shutdown();

    ID3D11Device *GetDevice() const;
    ID3D11DeviceContext *GetContext() const;

  private:
    bool CreateDevice(D3D_DRIVER_TYPE driverType);
    bool CreateRenderTarget();

  private:
    HWND m_Window = nullptr;

    ID3D11Device *m_Device = nullptr;
    ID3D11DeviceContext *m_Context = nullptr;
    IDXGISwapChain *m_SwapChain = nullptr;
    ID3D11RenderTargetView *m_RenderTarget = nullptr;
};