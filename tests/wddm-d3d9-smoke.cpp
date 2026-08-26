/*
 * Offscreen D3D9 smoke workload for the ARM64 WDDM bundle.
 *
 * This deliberately does not run in CI: it must execute beside the matching
 * DXVK and Turnip files on the target VM.  The workload clears a render target,
 * copies it to a system-memory surface, and verifies the readback.  That
 * exercises the D3D9 proxy, command submission, synchronization, and resource
 * transfers without requiring a visible window or a second renderer.
 */

#include <windows.h>
#include <d3d9.h>

#include <cstdio>

namespace
{

constexpr wchar_t kWindowClassName[] = L"DroidVmDxvkD3d9Smoke";

bool CreateTestWindow(HWND *window, bool *registered)
{
    if (window == nullptr || registered == nullptr)
    {
        return false;
    }

    *window = nullptr;
    *registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;

    ATOM atom = RegisterClassExW(&windowClass);
    if (atom == 0)
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }
    }
    else
    {
        *registered = true;
    }

    *window = CreateWindowExW(0,
                              kWindowClassName,
                              L"DroidVM DXVK D3D9 smoke",
                              WS_POPUP,
                              0,
                              0,
                              4,
                              4,
                              nullptr,
                              nullptr,
                              instance,
                              nullptr);
    if (*window == nullptr)
    {
        if (*registered)
        {
            UnregisterClassW(kWindowClassName, instance);
        }
        return false;
    }
    return true;
}

bool TryAdapter(IDirect3D9 *direct3D, UINT adapterIndex, HWND window)
{
    if (direct3D == nullptr || window == nullptr)
    {
        return false;
    }

    D3DPRESENT_PARAMETERS presentation = {};
    presentation.BackBufferWidth = 4;
    presentation.BackBufferHeight = 4;
    presentation.BackBufferFormat = D3DFMT_UNKNOWN;
    presentation.BackBufferCount = 1;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;
    presentation.Windowed = TRUE;
    presentation.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9 *device = nullptr;
    HRESULT status = direct3D->CreateDevice(adapterIndex,
                                             D3DDEVTYPE_HAL,
                                             window,
                                             D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                             &presentation,
                                             &device);
    IDirect3DSurface9 *renderTarget = nullptr;
    IDirect3DSurface9 *staging = nullptr;
    bool passed = false;

    if (SUCCEEDED(status))
    {
        status = device->CreateRenderTarget(4,
                                            4,
                                            D3DFMT_A8R8G8B8,
                                            D3DMULTISAMPLE_NONE,
                                            0,
                                            TRUE,
                                            &renderTarget,
                                            nullptr);
    }
    if (SUCCEEDED(status))
    {
        status = device->SetRenderTarget(0, renderTarget);
    }
    if (SUCCEEDED(status))
    {
        status = device->BeginScene();
    }
    if (SUCCEEDED(status))
    {
        status = device->Clear(0,
                               nullptr,
                               D3DCLEAR_TARGET,
                               D3DCOLOR_ARGB(255, 255, 0, 0),
                               1.0f,
                               0);
        HRESULT endStatus = device->EndScene();
        if (SUCCEEDED(status))
        {
            status = endStatus;
        }
    }
    if (SUCCEEDED(status))
    {
        status = device->CreateOffscreenPlainSurface(4,
                                                      4,
                                                      D3DFMT_A8R8G8B8,
                                                      D3DPOOL_SYSTEMMEM,
                                                      &staging,
                                                      nullptr);
    }
    if (SUCCEEDED(status))
    {
        status = device->GetRenderTargetData(renderTarget, staging);
    }
    if (SUCCEEDED(status))
    {
        D3DLOCKED_RECT locked = {};
        status = staging->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (SUCCEEDED(status))
        {
            if (locked.pBits != nullptr && locked.Pitch >= 4 * sizeof(BYTE))
            {
                const BYTE *pixel = static_cast<const BYTE *>(locked.pBits);
                passed = pixel[0] == 0x00 && pixel[1] == 0x00 && pixel[2] == 0xff && pixel[3] == 0xff;
            }
            staging->UnlockRect();
        }
    }

    if (staging != nullptr)
    {
        staging->Release();
    }
    if (renderTarget != nullptr)
    {
        renderTarget->Release();
    }
    if (device != nullptr)
    {
        device->Release();
    }
    return SUCCEEDED(status) && passed;
}

} // namespace

int main()
{
    HWND window = nullptr;
    bool registered = false;
    if (!CreateTestWindow(&window, &registered))
    {
        std::fprintf(stderr, "Failed to create the hidden D3D9 test window\n");
        return 1;
    }

    IDirect3D9 *direct3D = Direct3DCreate9(D3D_SDK_VERSION);
    bool passed = false;
    if (direct3D != nullptr)
    {
        for (UINT index = 0; index < direct3D->GetAdapterCount() && !passed; index++)
        {
            passed = TryAdapter(direct3D, index, window);
        }
        direct3D->Release();
    }
    else
    {
        std::fprintf(stderr, "Direct3DCreate9 failed\n");
    }

    DestroyWindow(window);
    if (registered)
    {
        UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    }
    if (!passed)
    {
        std::fprintf(stderr, "No adapter completed the D3D9 offscreen clear/readback\n");
    }
    return passed ? 0 : 2;
}
