#pragma once
#include <cstdint>
#include <string>

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// ゲームに影響する共有設定（ウィンドウ・起動シーン・時間・描画・衝突・入力）の JSON 入出力
  /// git で共有し、Debug / Release の両方で起動時に適用する
  /// </summary>
  namespace ProjectSettings {
    inline constexpr const char* kFilePath = "resources/Json/ProjectSettings.json";

    /// <summary>
    /// 起動時にだけ使うウィンドウ設定（ドラッグやフルスクリーンで変わる現在のサイズとは別に持つ）
    /// </summary>
    struct WindowSettings {
      std::string productName     = "TakoEngine";  ///< ウィンドウタイトル
      int32_t     width           = 1280;          ///< クライアント領域
      int32_t     height          = 720;
      bool        startFullscreen = false;
      bool        resizable       = true;
    };

    /// <summary>
    /// 編集・保存の対象になるウィンドウ設定（LoadWindow で更新される）
    /// </summary>
    WindowSettings& GetWindowSettings();

    /// <summary>
    /// Engine Settings の Save で名前から生成するヘッダ（作業ディレクトリからの相対パス。ファイル名が enum / namespace 名になる。空なら生成しない）
    /// </summary>
    struct GeneratedHeaders {
      std::string collisionLayers;  ///< 衝突層の enum
      std::string inputActions;     ///< 入力アクション名の定数
      std::string inputAxes;        ///< 入力軸名の定数
    };

    GeneratedHeaders& GetGeneratedHeaders();

    /// <summary>
    /// ウィンドウ設定だけを GetWindowSettings へ読み込む（WinApp には適用しない）
    /// </summary>
    /// <returns>読み込めたら true。ファイルが無い、または途中で失敗したら false</returns>
    bool LoadWindow();

    /// <summary>
    /// JSON を読み込み各システムへ適用する（ウィンドウ設定は LoadWindow が担当）
    /// </summary>
    /// <returns>適用できたら true。ファイルが無い、または途中で失敗したら false</returns>
    bool Load(DX12Basic* dx12);

    /// <summary>
    /// 各システムの現在値とウィンドウ設定を JSON に書き出す
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool Save(DX12Basic* dx12);
  }

} // namespace Tako
