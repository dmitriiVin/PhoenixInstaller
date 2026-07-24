#pragma once

#include <d3d11.h>
#include <windows.h>


class ImGuiManager {
  public:
    bool Initialize(HWND hwnd, ID3D11Device *device, ID3D11DeviceContext *context);

    void BeginFrame();

    void EndFrame();

    void Shutdown();
};