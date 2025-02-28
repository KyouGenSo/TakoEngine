#pragma once
#include <Windows.h>

class IWndProcHandler
{
public:
  virtual void OnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};
