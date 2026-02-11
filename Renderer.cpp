#include "Renderer.h"
#include "d3d12.h"
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <chrono>
#include <string>

using Microsoft::WRL::ComPtr;


// DirectX 12 objects
static ComPtr<ID3D12Device> device;
static ComPtr<IDXGISwapChain4> swapChain;
static ComPtr<ID3D12CommandQueue> commandQueue;
static ComPtr<ID3D12DescriptorHeap> rtvHeap;
static ComPtr<ID3D12Resource> renderTargets[2];
static ComPtr<ID3D12CommandAllocator> commandAllocator;
static ComPtr<ID3D12GraphicsCommandList> commandList;
static ComPtr<ID3D12Fence> fence;
static HANDLE fenceEvent;
static UINT frameIndex;
static UINT rtvDescriptorSize;

// Direct2D/DirectWrite
static ComPtr<ID2D1Factory> d2dFactory;
static ComPtr<ID2D1HwndRenderTarget> d2dRenderTarget;
static ComPtr<IDWriteFactory> dwriteFactory;
static ComPtr<IDWriteTextFormat> textFormat;

// Timer state
static std::chrono::steady_clock::time_point startTime;
static int focusMinutes = 60;
static int breakMinutes = 15;
static int rounds = 4;
static int currentRound = 1;
static bool inFocus = true;
static std::wstring timerText;

static double progressFraction = 1.0;

bool Renderer::Initialize(HWND hwnd) {
    HRESULT hr;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.BufferCount = 2;
    swapDesc.Width = 800;
    swapDesc.Height = 600;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    hr = factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapDesc, nullptr, nullptr, &tempSwapChain);
    if (FAILED(hr)) return false;

    tempSwapChain.As(&swapChain);
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) return false;

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; i++) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) return false;
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    if (FAILED(hr)) return false;

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) return false;
    commandList->Close();

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) return false;
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2dFactory));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = dwriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.0f, L"en-us", &textFormat
    );
    if (FAILED(hr)) return false;

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps =
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(400, 100));

    hr = d2dFactory->CreateHwndRenderTarget(rtProps, hwndProps, &d2dRenderTarget);
    if (FAILED(hr)) return false;

    startTime = std::chrono::steady_clock::now();
    timerText = L"Focus - " + std::to_wstring(focusMinutes) + L":00";

    return true;
}

void Renderer::Update()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

    int totalSeconds = (inFocus ? focusMinutes : breakMinutes) * 60;
    int remaining = totalSeconds - (int)elapsed;
    progressFraction = static_cast<double>(remaining) / totalSeconds;

    if (remaining <= 0)
    {
        // Switch phase
        inFocus = !inFocus;
        if (inFocus)
        {
            currentRound++;
        }
        startTime = std::chrono::steady_clock::now();
        remaining = (inFocus ? focusMinutes : breakMinutes) * 60;
    }

    int minutes = remaining / 60;
    int seconds = remaining % 60;

    std::wstring prefix = inFocus ? L"Focus - " : L"Break - ";

    std::wstring minStr = (minutes < 10 ? L"0" : L"") + std::to_wstring(minutes);
    std::wstring secStr = (seconds < 10 ? L"0" : L"") + std::to_wstring(seconds);

    timerText = prefix + minStr + L":" + secStr;
}

void Renderer::Render() {
    d2dRenderTarget->BeginDraw();

    // Bar dimensions (define them here so they exist)
    float barX = 20.0f;
    float barY = 30.0f;
    float barWidth = 360.0f;
    float barHeight = 40.0f;

    // Background bar (semi‑transparent gray)
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 0.5f), &bgBrush);
    D2D1_RECT_F bgRect = D2D1::RectF(barX, barY, barX + barWidth, barY + barHeight);
    d2dRenderTarget->FillRectangle(&bgRect, bgBrush.Get());

    // Foreground bar (shrinks with progressFraction)
    ComPtr<ID2D1SolidColorBrush> fgBrush;
    D2D1::ColorF color = inFocus ? D2D1::ColorF(D2D1::ColorF::Green) : D2D1::ColorF(D2D1::ColorF::Blue);
    d2dRenderTarget->CreateSolidColorBrush(color, &fgBrush);
    float filledWidth = static_cast<float>(barWidth * progressFraction);
    D2D1_RECT_F fgRect = D2D1::RectF(barX, barY, barX + filledWidth, barY + barHeight);
    d2dRenderTarget->FillRectangle(&fgRect, fgBrush.Get());

    // Auto‑resize font based on bar height
    float fontSize = barHeight * 0.6f; // scale relative to bar
    ComPtr<IDWriteTextFormat> dynamicFormat;
    dwriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-us",
        &dynamicFormat
    );
    dynamicFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    dynamicFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Text rectangle matches bar
    D2D1_RECT_F textRect = D2D1::RectF(barX, barY, barX + barWidth, barY + barHeight);

    // Text brush (white)
    ComPtr<ID2D1SolidColorBrush> textBrush;
    d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &textBrush);

    // Draw time remaining inside bar
    d2dRenderTarget->DrawText(
        timerText.c_str(),
        (UINT32)timerText.length(),
        dynamicFormat.Get(),
        &textRect,
        textBrush.Get()
    );

    HRESULT hr = d2dRenderTarget->EndDraw();
    if (FAILED(hr)) OutputDebugString(L"Direct2D EndDraw failed\n");
}

//void Renderer::Render() {
//    d2dRenderTarget->BeginDraw();
//
//    // Progress bar background
//    float barX = 20.0f, barY = 30.0f, barWidth = 360.0f, barHeight = 20.0f;
//    ComPtr<ID2D1SolidColorBrush> bgBrush;
//    d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 0.5f), &bgBrush);
//    D2D1_RECT_F bgRect = D2D1::RectF(barX, barY, barX + barWidth, barY + barHeight);
//    d2dRenderTarget->FillRectangle(&bgRect, bgBrush.Get());
//
//    // Foreground bar
//    ComPtr<ID2D1SolidColorBrush> fgBrush;
//    D2D1::ColorF color = inFocus ? D2D1::ColorF(D2D1::ColorF::Green) : D2D1::ColorF(D2D1::ColorF::Blue);
//    d2dRenderTarget->CreateSolidColorBrush(color, &fgBrush);
//    float filledWidth = static_cast<float>(barWidth * progressFraction);
//    D2D1_RECT_F fgRect = D2D1::RectF(barX, barY, barX + filledWidth, barY + barHeight);
//    d2dRenderTarget->FillRectangle(&fgRect, fgBrush.Get());
//
//    // Time remaining text inside the bar
//    ComPtr<ID2D1SolidColorBrush> textBrush;
//    d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &textBrush);
//    D2D1_RECT_F textRect = D2D1::RectF(barX, barY, barX + barWidth, barY + barHeight);
//    d2dRenderTarget->DrawText(
//        timerText.c_str(),
//        (UINT32)timerText.length(),
//        textFormat.Get(),
//        &textRect,
//        textBrush.Get()
//    );
//
//    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
//    textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
//
//    HRESULT hr = d2dRenderTarget->EndDraw();
//    if (FAILED(hr)) OutputDebugString(L"Direct2D EndDraw failed\n");
//}
