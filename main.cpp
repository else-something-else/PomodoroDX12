#include <windows.h>
#include "App.h"
#include "Renderer.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
    case WM_TIMER:
        Renderer::Update();                     // update countdown logic
        InvalidateRect(hwnd, nullptr, FALSE);   // force redraw
        return 0;

    case WM_NCHITTEST: {
        // Treat the whole client area as draggable
        LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT) return HTCAPTION;
        return hit;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PomodoroDX12Class";

    RegisterClass(&wc);

    // Create the window
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,   // layered + always on top
        L"PomodoroDX12Class",
        nullptr,                             // no title
        WS_POPUP,                            // borderless
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 100,                            // width, height (just enough for bar)
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) return 0;

    // Make window transparent
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetTimer(hwnd, 1, 1000, nullptr);

    // Initialize renderer
    if (!Renderer::Initialize(hwnd)) return 0;

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        Renderer::Update();
        Renderer::Render();
    }

    return (int)msg.wParam;
}