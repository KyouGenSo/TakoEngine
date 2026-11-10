#pragma once
#include "Component.h"
#include "Vector3.h"

#include <memory>
#include <string>

namespace Tako {

  class EmitterManager;
  class GPUParticleEmitter;

  /// <summary>
  /// パーティクルのプリセットを、Actor の位置から出す（Unity の ParticleSystem、UE の NiagaraComponent）。
  /// エミッターはシーンの EmitterManager に置き、Play 中だけ出す
  /// </summary>
  TCLASS()
  class ParticleSystemComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    ~ParticleSystemComponent() override;

    void BeginPlay() override;

    void EndPlay() override;

    /// <summary>
    /// プリセットが変わっていれば読み直し、エミッターを Actor の位置へ動かす
    /// </summary>
    void Draw() override;

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// 出す・止める（Play 中だけ出る）
    /// </summary>
    void SetActive(bool active);

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 数や頻度を変えるなど、スクリプトから使う。まだ作っていなければここで作るので、BeginPlay からも使える
    /// </summary>
    /// <returns>プリセットが空か、読めなければ nullptr</returns>
    std::shared_ptr<GPUParticleEmitter> GetEmitter();

  private: //非公開関数
    /// <summary>
    /// プリセットの名前が変わっていれば、エミッターを作り直す
    /// </summary>
    void EnsureLoaded();

    void RemoveEmitter();

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "ParticlePreset")
    std::string preset_;
    TPROPERTY(Edit, Tooltip = "Play が始まったら出す（オフならスクリプトから SetEmitterActive で出す）")
    bool        playOnStart_ = true;
    TPROPERTY(Edit, Tooltip = "Actor からの位置（Actor の回転と大きさが掛かる）", Speed = 0.05)
    Vector3     offset_      = {};

    EmitterManager* manager_      = nullptr;  ///< emitterName_ を置いた先（シーンの物。Actor より後に消える）
    std::string     emitterName_;
    std::string     loadedPreset_;
    bool            isPlaying_    = false;
    bool            isActive_     = false;    ///< 出している（playOnStart か SetActive）
  };

} // namespace Tako
