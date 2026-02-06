#include <windows.h>
#include <string>

#include "RenderView.h"

using SR::CRenderView;

namespace {

/**
 * @brief 启用高 DPI 感知，支持 4K 等高分辨率显示器
 */
void EnableDpiAwareness() {
    // 优先尝试启用 Per-Monitor V2 模式
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setContext = reinterpret_cast<SetDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext) {
            setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }

    // 旧版本系统回退方案
    SetProcessDPIAware();
}

/**
 * @brief 获取系统当前的 DPI 缩放级别
 */
UINT GetSystemDpi() {
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        using GetDpiForSystemFn = UINT(WINAPI*)();
        auto getDpi = reinterpret_cast<GetDpiForSystemFn>(
            GetProcAddress(user32, "GetDpiForSystem"));
        if (getDpi) {
            UINT dpi = getDpi();
            FreeLibrary(user32);
            return dpi;
        }
        FreeLibrary(user32);
    }
    HDC screen = GetDC(nullptr);
    UINT dpi = screen ? static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSX)) : 96u;
    if (screen) {
        ReleaseDC(nullptr, screen);
    }
    return dpi;
}

/**
 * @brief Win32 窗口过程函数，处理各类系统消息并将其转发给 CRenderView
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    CRenderView* view = reinterpret_cast<CRenderView*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        // 创建渲染器视图对象并初始化 HDR
        auto* newView = new CRenderView();
        newView->InitializeHDR(hwnd, width, height);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(newView));
        return 0;
    }
    case WM_SIZE: {
        if (view) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            view->Resize(width, height);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        if (view) {
            view->DrawHDR();

            // 更新窗口标题文字，包含帧率信息
            static float lastFps = -1.0f;
            float currentFps = view->GetFPS();
            if (std::abs(currentFps - lastFps) > 0.1f) {
                wchar_t title[128];
                swprintf_s(title, L"SoftRasterizer MFCDemo [FPS: %.1f]", currentFps);
                SetWindowTextW(hwnd, title);
                lastFps = currentFps;
            }
        }
        EndPaint(hwnd, &ps);
        // 实现持续重绘循环
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hwnd, nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (view) {
            SetCapture(hwnd);
            int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            view->OnMouseDown(x, y, true);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (view) {
            view->OnMouseUp(true);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        if (view) {
            SetCapture(hwnd);
            int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            view->OnMouseDown(x, y, false);
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        if (view) {
            view->OnMouseUp(false);
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (view) {
            int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            view->OnMouseMove(x, y);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (view) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            view->OnMouseWheel(delta);
        }
        return 0;
    }
    case WM_DESTROY: {
        delete view;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

} // namespace

/**
 * @brief Win32 程序的入口点
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 设置高 DPI 支持
    EnableDpiAwareness();

    const wchar_t className[] = L"SoftRasterizerDemo";

    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        return 0;
    }

    // 根据 DPI 缩放计算初始窗口大小
    UINT dpi = GetSystemDpi();
    int baseW = 1024;
    int baseH = 768;
    int scaledW = MulDiv(baseW, dpi, 96);
    int scaledH = MulDiv(baseH, dpi, 96);

    HWND hwnd = CreateWindowEx(
        0,
        className,
        L"SoftRasterizer MFCDemo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        scaledW, scaledH,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 标准 Win32 消息循环
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
