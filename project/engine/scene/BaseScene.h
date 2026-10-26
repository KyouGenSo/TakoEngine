#pragma once

namespace Tako {

  /// <summary>
  /// 全てのシーンの基底クラス。初期化、更新、描画のインターフェースを定義
  /// </summary>
  class BaseScene
  {
  public: //メンバー関数

    virtual ~BaseScene() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    virtual void Initialize() = 0;

    /// <summary>
    /// 終了処理
    /// </summary>
    virtual void Finalize() = 0;

    /// <summary>
    /// 更新
    /// </summary>
    virtual void Update() = 0;

    /// <summary>
    /// 描画物を登録する。描画キューが kQueueOverlay 以上の物（既定のスプライト）はポストエフェクトの後に描かれる
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// ImGui の描画
    /// </summary>
    virtual void DrawImGui() = 0;

  };

} // namespace Tako