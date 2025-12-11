#pragma once
#include <Windows.h>

namespace Tako {

class IWndProcHandler
{
public:
  virtual void OnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};

} // namespace Tako
