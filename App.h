#pragma once
#include <windows.h>

class App
{
public:
	App(HINSTANCE hInstance);
	bool Initialize(int nCmdShow);
	int Run();

private:
	HINSTANCE hInstance;
	HWND hwnd;

	bool InitWindow(int nCmdShow);
	void MainLoop();
};