#pragma once

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// ゲームに影響する共有設定（VSync・FPS 上限・影）の JSON 入出力
  /// git で共有し、Debug / Release の両方で起動時に適用する
  /// </summary>
  namespace ProjectSettings {
    inline constexpr const char* kFilePath = "resources/Json/ProjectSettings.json";

    /// <summary>
    /// JSON を読み込み各システムへ適用する
    /// </summary>
    /// <returns>適用できたら true。ファイルが無い、または途中で失敗したら false</returns>
    bool Load(DX12Basic* dx12);

    /// <summary>
    /// 各システムの現在値を JSON に書き出す
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool Save(DX12Basic* dx12);
  }

} // namespace Tako
