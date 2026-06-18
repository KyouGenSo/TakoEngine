#pragma once
#include <Windows.h>

namespace Tako {

  /// <summary>
  /// ウィンドウプロシージャ通知を受け取るハンドラインターフェース
  /// </summary>
  class IWndProcHandler
  {
  public: //メンバー関数
    virtual void OnWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
  };

} // namespace Tako
