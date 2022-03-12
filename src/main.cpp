#include <stdio.h>
#include <Windows.h>
#include <d3d11_1.h>

#ifdef NDEBUG
#undef assert
#define assert(x)
#endif

#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_internal.h>
#include <renderdoc/renderdoc.h>
#include <superluminal/PerformanceAPI.h>
#include "gui.h"
#include "Threading.h"



static ID3D11Device *g_pd3dDevice = NULL;
static ID3D11DeviceContext *g_pd3dDeviceContext = NULL;
static IDXGISwapChain1 *g_pSwapChain = NULL;
static ID3D11RenderTargetView *g_mainRenderTargetView = NULL;

ID3D11ShaderResourceView *CA_Texture;
ID3D11ShaderResourceView *Action_Slots;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImVec4 clear_color = ImVec4(0.192f, 0.192f, 0.192f, 1.00f);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int imgui_main();

HWND hwnd;
DWORD gui_thread;

#include <intrin.h>

int main(int argc, char **argv) {
    PerformanceAPI_SetCurrentThreadName("Message Loop Thread");

    // Create application window
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MainWndProc, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, L"FFTools", NULL };
    ::RegisterClassExW(&wc);
    hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"FFTools", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);
    
    if (!hwnd) {
        printf("Error: CreateWindowExW failed with code %lx.\n", GetLastError());
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    auto gui_thread_handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)imgui_main, 0, 0, &gui_thread);

    bool done = false;
    while (!done && WaitForSingleObject(gui_thread_handle, 0) != WAIT_OBJECT_0) {
        PERFORMANCEAPI_INSTRUMENT("Process Messages");

        HANDLE wait_handles[] = { gui_thread_handle };
        MsgWaitForMultipleObjects(IM_ARRAYSIZE(wait_handles), wait_handles, FALSE, 2000, QS_ALLINPUT | QS_ALLPOSTMESSAGE);

        MSG msg;
        while (::PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
    }

    WaitForSingleObject(gui_thread_handle, 30000);

    return 0;
}

//#pragma comment(lib, "clang-rt.lib")

int imgui_main() {
    PerformanceAPI_SetCurrentThreadName("GUI Thread");

    Renderdoc::init();

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto imgui_context = ImGui::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    auto init_error = GUI::init(g_pd3dDevice);
    if (init_error) {
        CleanupDeviceD3D();
        MessageBoxA(hwnd, init_error, "FFTools", MB_OK | MB_ICONERROR);
        return 1;
    }

    #ifdef _DEBUG
    bool show_demo_window = !true;
    #else
    bool show_demo_window = false;
    #endif

    // Main loop
    bool done = false;
    while (!done) {
        PERFORMANCEAPI_INSTRUMENT("Frame Loop");
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        {
            PERFORMANCEAPI_INSTRUMENT("Process Messages");

            MSG msg;
            while (::PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);

                if (ImGui_ImplWin32_WndProcHandler(hwnd, msg.message, msg.wParam, msg.lParam))
                    continue;

                switch (msg.message) {
                    case WM_SIZE:
                        if (g_pd3dDevice != NULL && msg.wParam != SIZE_MINIMIZED) {
                            CleanupRenderTarget();
                            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(msg.lParam), (UINT)HIWORD(msg.lParam), DXGI_FORMAT_UNKNOWN, 0);
                            CreateRenderTarget();
                        }
                        break;

                    case WM_CLOSE:
                    case WM_DESTROY:
                        done = true;
                        break;
                }
            }
        }

        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        imgui_context->CurrentWindow->Flags |= ImGuiWindowFlags_NoSavedSettings;

        done = GUI::per_frame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);

    return 0;
}


// Helper functions

bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
    /*DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;*/

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = 2;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = 0;// DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Scaling = DXGI_SCALING_NONE;
    
    UINT createDeviceFlags = 0;
    #ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    IDXGIDevice1 *device = 0;
    if(g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&device)) != S_OK)
       return false;

    device->SetMaximumFrameLatency(1);

    IDXGIAdapter2 *adapter = 0;
    if (device->GetParent(IID_PPV_ARGS(&adapter)) != S_OK)
        return false;

    IDXGIFactory2 *factory = 0;
    if (adapter->GetParent(IID_PPV_ARGS(&factory)) != S_OK)
        return false;

    HRESULT hr = 0;
    if ((hr = factory->CreateSwapChainForHwnd(g_pd3dDevice, hwnd, &sd, 0, 0, &g_pSwapChain)) != S_OK)
        return false;

    factory->Release();
    adapter->Release();
    device->Release();

    DXGI_RGBA background_color = {};
    background_color.a = clear_color.w;
    background_color.r = clear_color.x;
    background_color.g = clear_color.y;
    background_color.b = clear_color.z;
    g_pSwapChain->SetBackgroundColor(&background_color);

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget() {
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

extern bool ImGui_ImplWin32_UpdateMouseCursor();

LRESULT WINAPI MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostThreadMessageW(gui_thread, msg, wParam, lParam);
            PostQuitMessage(0);
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;

        case WM_SETCURSOR:
            if(GImGui)
                if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
                    return 1;
            break;

        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KILLFOCUS:
        case WM_CHAR:
        case WM_DEVICECHANGE:
        case WM_SIZE:
            PostThreadMessage(gui_thread, msg, wParam, lParam);
            break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    return main(0, 0);
}