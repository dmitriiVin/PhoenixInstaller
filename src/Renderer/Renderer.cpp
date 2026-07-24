#include "Renderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

bool Renderer::Initialize(HWND hwnd) {
    m_Window = hwnd;

    if (CreateDevice(D3D_DRIVER_TYPE_HARDWARE))
        return true;

    if (CreateDevice(D3D_DRIVER_TYPE_WARP))
        return true;

    if (CreateDevice(D3D_DRIVER_TYPE_REFERENCE))
        return true;

    return false;
}

bool Renderer::CreateDevice(D3D_DRIVER_TYPE type) {
    DXGI_SWAP_CHAIN_DESC sd{};

    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_Window;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // Современный SwapEffect
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, nullptr, &m_Context);

    if (FAILED(hr))
        return false;

    return CreateRenderTarget();
}

bool Renderer::CreateRenderTarget() {
    ID3D11Texture2D *backBuffer = nullptr;

    HRESULT hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    if (FAILED(hr))
        return false;

    hr = m_Device->CreateRenderTargetView(backBuffer, nullptr, &m_RenderTarget);

    backBuffer->Release();

    return SUCCEEDED(hr);
}

void Renderer::BeginFrame() {
    if (!m_RenderTarget)
        return;

    static constexpr float clearColor[4] = {0.10f, 0.10f, 0.10f, 1.0f};

    m_Context->OMSetRenderTargets(1, &m_RenderTarget, nullptr);

    m_Context->ClearRenderTargetView(m_RenderTarget, clearColor);
}

void Renderer::Resize(UINT width, UINT height) {
    if (!m_SwapChain)
        return;

    if (width == 0 || height == 0)
        return;

    if (m_RenderTarget) {
        m_RenderTarget->Release();
        m_RenderTarget = nullptr;
    }

    m_Context->OMSetRenderTargets(0, nullptr, nullptr);

    HRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(hr))
        return;

    CreateRenderTarget();
}

void Renderer::EndFrame() {
    if (m_SwapChain)
        m_SwapChain->Present(1, 0);
}

void Renderer::Shutdown() {
    if (m_RenderTarget) {
        m_RenderTarget->Release();
        m_RenderTarget = nullptr;
    }

    if (m_SwapChain) {
        m_SwapChain->Release();
        m_SwapChain = nullptr;
    }

    if (m_Context) {
        m_Context->Release();
        m_Context = nullptr;
    }

    if (m_Device) {
        m_Device->Release();
        m_Device = nullptr;
    }
}

ID3D11Device *Renderer::GetDevice() const {
    return m_Device;
}

ID3D11DeviceContext *Renderer::GetContext() const {
    return m_Context;
}