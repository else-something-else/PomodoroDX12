#pragma once
#include <windows.h>

class Renderer
{
public:
	static bool Initialize(HWND hwnd);
	static void Update();
	static void Render();
	static void Shutdown();
};