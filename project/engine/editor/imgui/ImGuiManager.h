#pragma once

#ifdef _DEBUG

#include <cstdint>

#include"imgui.h"
#include "IconsFontAwesome6.h"
#include <d3d12.h>
#include<wrl.h>

namespace Tako {

  class WinApp;
  class DX12Basic;

  /// <summary>
  /// ImGui の DirectX 12統合管理クラス。初期化、描画、スタイル設定を担当
  /// </summary>
  class TAKO_API ImGuiManager
  {
  public: //定数
    // 大きいアイコンのフォントに焼く字。GetLargeIconFont で描くアイコンはここに足す
    static constexpr const char* kLargeIcons =
      ICON_FA_FOLDER ICON_FA_FILE ICON_FA_FILE_CODE ICON_FA_FILE_LINES ICON_FA_CUBE ICON_FA_CUBES ICON_FA_MUSIC ICON_FA_IMAGE
      ICON_FA_FILM ICON_FA_PALETTE ICON_FA_WAND_MAGIC_SPARKLES ICON_FA_DIAGRAM_PROJECT ICON_FA_TABLE_COLUMNS ICON_FA_VIDEO ICON_FA_FONT
      ICON_FA_MOUNTAIN_SUN ICON_FA_PERSON_RUNNING;

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
    /// 大きいアイコン（Assets の一覧など）を描くフォント。kLargeIcons の字だけを持つ
    /// </summary>
    /// <returns>フォントのファイルが揃っていなければ、普通のフォント（アイコンは '?' になる）</returns>
    static ImFont* GetLargeIconFont() { return largeIconFont_ ? largeIconFont_ : ImGui::GetFont(); }

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
    bool needsDefaultLayout_ = false;  ///< 配置ファイル（imgui.ini）が無い。最初のフレームで既定の並びを作る

    uint32_t fontSrvIndex_ = 0;  ///< フォント用の SRV インデックス（SrvManager で確保）

    static inline ImFont* largeIconFont_ = nullptr;

  };

} // namespace Tako

#endif // _DEBUG