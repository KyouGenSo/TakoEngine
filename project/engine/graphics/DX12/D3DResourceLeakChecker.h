#pragma once

namespace Tako {

  /// <summary>
  /// 破棄時に DXGI デバッグ層へ存命 D3D オブジェクトを報告するリークチェッカー
  /// </summary>
  class D3DResourceLeakChecker {
  public: //メンバー関数
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~D3DResourceLeakChecker();
  };

} // namespace Tako