#include "App.h"
#include "Renderer.h"

App::App(HINSTANCE hInstance) : hInstance(hInstance), hwnd(nullptr) {}

bool App::InitWindow(int nCmdShow)
{
	WNDCLASS wc = {};
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"PomodoroDX12";

	RegisterClass(&wc);

	ShowWindow(hwnd, nCmdShow);
	return true;
}

bool App::Initialize(int nCmdShow)
{
	if (!InitWindow(nCmdShow)) return false;
	Renderer::Initialize(hwnd);
	return true;
}

int App::Run()
{
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			MainLoop();
		}
	}
	return (int)msg.wParam;
}

void App::MainLoop()
{
	Renderer::Update();
	Renderer::Render();
}