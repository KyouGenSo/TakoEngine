#pragma once

#ifdef _DEBUG

#include <cstdint>

#include"imgui.h"
#include <d3d12.h>
#include<wrl.h>

namespace Tako {

  class WinApp;
  class DX12Basic;

  /// <summary>
  /// ImGui の DirectX 12統合管理クラス。初期化、描画、スタイル設定を担当
  /// </summary>
  class ImGuiManager
  {
  public: //メンバー関数

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(WinApp* winApp, DX12Basic* dx12, bool isDocking);

    /// <summary>
    /// dx12用初期化
    /// </summary>
    void InitializeForDX12();

    /// <summary>
    /// 描画開始
    /// </summary>
    void Begin();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 描画終了
    /// </summary>
    void End();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Shutdown();

    /// <summary>
    /// ウィンドウサイズ変更時の更新処理
    /// </summary>
    void OnWindowResize();

    /// <summary>
    /// ImGui のスタイルの設定
    /// </summary>
    void SetStyleMoonLight();

    /// <summary>
    /// docking 設定
    /// </summary>
    void SetDocking(bool isDocking);

    /// <summary>
    /// DockSpaceViewPort の設定
    /// </summary>
    void SetDockSpaceViewPort();

  private: //メンバー変数

    WinApp* winApp_ = nullptr;  ///< WinApp クラスのインスタンス

    DX12Basic* dx12_ = nullptr;  ///< DX12Basic クラスのインスタンス

    bool isDocking_ = false;

    uint32_t fontSrvIndex_ = 0;  ///< フォント用の SRV インデックス（SrvManager で確保）

  };

} // namespace Tako

#endif // _DEBUG