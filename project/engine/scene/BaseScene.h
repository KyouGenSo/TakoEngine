#pragma once

namespace Tako {

  /// <summary>
  /// 全てのシーンの基底クラス。初期化、更新、描画のインターフェースを定義
  /// </summary>
  class BaseScene
  {
  public: // メンバ関数

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
    /// 描画
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// エフェクトなしで描画
    /// </summary>
    virtual void DrawWithoutEffect() = 0;

    /// <summary>
    /// ImGui の描画
    /// </summary>
    virtual void DrawImGui() = 0;

  };

} // namespace Tako