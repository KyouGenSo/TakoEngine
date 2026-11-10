#pragma once
#include "Component.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Tako {

  /// <summary>
  /// 音を鳴らす（Unity の AudioSource、UE の AudioComponent。3D の定位は無い）。Play が始まったら鳴らし、終わったら止める
  /// </summary>
  TCLASS()
  class AudioSourceComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    void BeginPlay() override;

    void EndPlay() override;

    /// <summary>
    /// 鳴っている音の音量を変える
    /// </summary>
    void PostEditChange() override;

    /// <summary>
    /// 最初から鳴らす（鳴っていれば止めてから）。スクリプトから呼ぶ
    /// </summary>
    void Play();

    void Stop();

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "Sound")
    std::string sound_;
    TPROPERTY(Edit, ClampMin = 0, ClampMax = 1, Speed = 0.01)
    float       volume_      = 1.0f;
    TPROPERTY(Edit)
    bool        loop_        = false;
    TPROPERTY(Edit)
    bool        playOnStart_ = true;

    std::optional<uint32_t> voice_;  ///< 鳴らしている音（0 も有効なハンドル）
  };

} // namespace Tako
