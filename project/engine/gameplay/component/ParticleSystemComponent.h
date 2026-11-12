#pragma once
#include "Component.h"
#include "Vector3.h"

#include <memory>
#include <string>

namespace Tako {

  class EmitterManager;
  class GPUParticleEmitter;
  class Model;
  class Object3d;

  /// <summary>
  /// パーティクルのプリセットを、Actor の位置から出す（Unity の ParticleSystem、UE の NiagaraComponent）。
  /// エミッターはシーンの EmitterManager に置き、Play 中だけ出す。
  /// Emitter Name を決めておくと、スクリプトは EmitterManager からその名前で引いて、出す・止める・動かすを行える。
  /// Bind To Mesh をオンにすると、Mesh エミッターのプリセットを、同じ Actor の MeshComponent のモデルの表面から出す
  /// </summary>
  TCLASS(DisplayName = "Particle System")
  class TAKO_API ParticleSystemComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    ~ParticleSystemComponent() override;

    void Awake() override;

    void OnActiveChanged(bool active) override;

    /// <summary>
    /// 結び付けた MeshComponent が外されるなら、エミッターを消す
    /// </summary>
    void OnSiblingRemoved(Component* removed) override;

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

    /// <summary>
    /// このエミッターの写しを今の位置に作り、lifeTime 秒だけ出す（爆発など 1 回きりの演出）。コンポーネントが消えても最後まで出る
    /// </summary>
    void EmitTemporary(float lifeTime);

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

    Vector3 GetEmitPosition() const;

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "ParticlePreset")
    std::string preset_;
    TPROPERTY(Edit, Tooltip = "スクリプトがシーンの EmitterManager から引くときの名前。空なら自動で付ける（同じ名前を 2 つ置かない）")
    std::string emitterName_;
    TPROPERTY(Edit, Tooltip = "Play が始まったら出す（オフならスクリプトから SetEmitterActive で出す）")
    bool        playOnStart_ = true;
    TPROPERTY(Edit, Tooltip = "Play 中も Actor の位置へ動かし続ける。オフなら、Play 中の位置はスクリプトが EmitterManager で決める")
    bool        followActor_ = true;
    TPROPERTY(Edit, Tooltip = "Actor からの位置（Actor の回転と大きさが掛かる）", Speed = 0.05)
    Vector3     offset_      = {};
    TPROPERTY(Edit, Tooltip = "Mesh エミッターのプリセットを、同じ Actor の MeshComponent のモデルの表面から出し、その動きに付いていかせる（位置は MeshComponent が決める）。モデルが入るまでは作らずに待つ")
    bool        bindToMesh_  = false;

    EmitterManager*                   manager_           = nullptr;  ///< activeName_ を置いた先（シーンの物。Actor より後に消える）
    std::string                       activeName_;                   ///< 今のエミッターの名前（emitterName_ か、自動で付けた名前）
    std::weak_ptr<GPUParticleEmitter> created_;                      ///< 自分が作ったエミッター（同じ名前の別物と見分ける）
    std::string                       loadedPreset_;
    std::string                       loadedEmitterName_;
    bool                              loadedBindToMesh_  = false;
    Object3d*                         boundObject_       = nullptr;  ///< 結び付けた MeshComponent の描画物（結び付けていなければ nullptr）
    const Model*                      boundModel_        = nullptr;  ///< 結び付けたときのモデル。替わったら作り直す
    bool                              isPlaying_         = false;
    bool                              isActive_          = false;    ///< 出している（playOnStart か SetActive）
  };

} // namespace Tako
