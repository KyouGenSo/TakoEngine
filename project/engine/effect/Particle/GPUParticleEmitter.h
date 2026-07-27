#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "ParticleStruct.h"
#include <memory>
#include <string>
#include <json_fwd.hpp>

namespace Tako {

  // 前方宣言
  class GPUParticle;
  class Mesh;

  /// <summary>
  /// GPU パーティクルエミッター基底クラス
  /// 球体、箱型、三角形など各種エミッターの共通機能を提供
  /// パーティクル射出タイミング制御、色・速度・スケールの範囲指定、一時エミッター機能をサポート
  /// std::enable_shared_from_this により安全な shared_ptr の生成を実現
  /// </summary>
  class GPUParticleEmitter : public std::enable_shared_from_this<GPUParticleEmitter>
  {
  public: //メンバー関数
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    /// <param name="emitterId">エミッター ID</param>
    GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId);

    /// <summary>
    /// デストラクタ
    /// </summary>
    virtual ~GPUParticleEmitter();

    /// <summary>
    /// エミッターの複製を作成
    /// </summary>
    /// <returns>複製されたエミッター</returns>
    virtual std::shared_ptr<GPUParticleEmitter> Clone() const = 0;

    /// <summary>
    /// 型固有パラメータを json に書き出す
    /// </summary>
    /// <param name="json">出力先の JSON オブジェクト</param>
    /// <remarks>
    /// 共通パラメータの保存は <c>EmitterManager::SerializeEmitterToJSON</c> が担当。
    /// 復元側の対は各派生クラスの static <c>CreateFromJSON</c> と <c>DeserializeTypeSpecific</c>。
    /// </remarks>
    virtual void SerializeTypeSpecific(nlohmann::json& json) const = 0;

    /// <summary>
    /// json から型固有パラメータを読み込んで自身へ適用する
    /// </summary>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <remarks>
    /// <c>SerializeTypeSpecific</c> の対。該当キーが無いパラメータは変更しない。
    /// スポーン形状の GPU リソース (Mesh の SRV 群) や Object3d バインドは再構築・変更しない。
    /// </remarks>
    virtual void DeserializeTypeSpecific(const nlohmann::json& json) = 0;

    /// <summary>
    /// エミッターの射出更新
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <remarks>派生クラスが追加の同期処理を行えるよう virtual。基底実装で active/emit タイマーと動的同期を実行する</remarks>
    virtual void UpdateEmission(float deltaTime);

    /// <summary>
    /// 収束目標座標を動的にバインド
    /// </summary>
    /// <param name="pPosition">毎フレーム読み取られる Vector3 へのポインタ。ライフタイム管理は呼び出し側責務</param>
    /// <remarks>
    /// 非 nullptr のとき、<c>UpdateEmission()</c> 内で毎フレーム <c>*pPosition</c> を <c>data_.targetPosition</c> に同期する。
    /// 解除するには <c>UnbindTargetPosition()</c> または <c>BindTargetPosition(nullptr)</c>。
    /// </remarks>
    void BindTargetPosition(const Vector3* pPosition);

    /// <summary>
    /// 動的バインドを解除
    /// </summary>
    void UnbindTargetPosition() { boundTargetPosition_ = nullptr; }

    /// <summary>
    /// 指定したパラメータのランダム化を有効化（既存ビットは保持）
    /// </summary>
    /// <param name="mask">有効化したい ERAND_* のビット OR</param>
    void EnableRandom(uint32_t mask);

    /// <summary>
    /// 指定したパラメータのランダム化を無効化（既存ビットは保持）
    /// </summary>
    /// <param name="mask">無効化したい ERAND_* のビット OR</param>
    void DisableRandom(uint32_t mask);

    /// <summary>
    /// 一時的なエミッターの寿命を更新
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateTemporaryLifeTime(float deltaTime);

    /// <summary>
    /// 描画モデルをデフォルトの板ポリに戻す。
    /// </summary>
    void ResetParticleModel();

    /// <summary>
    /// テクスチャを既定 (circle.dds) に戻す。
    /// </summary>
    void ResetTexture() { data_.textureSrvIndex = 0; }

    //============================================================
    //Setter
    //============================================================
    void SetPosition(const Vector3& position);
    void SetActive(bool isActive);
    void SetEmitting(bool cond);
    void SetNormalize(bool isNormalize);
    void SetRandomRotateZ(bool isRandomRotateZ);
    void SetUseForceField(bool useForceField);
    void SetUseCurlNoise(bool useCurlNoise);
    void SetUseDepthCollision(bool useDepthCollision);
    void SetParticleCount(uint32_t count);
    void SetFrequency(float frequency);
    void SetFrequencyTime(float frequencyTime);
    void SetColor(const Vector4& color);
    void SetStartColor(const Vector4& color);
    void SetEndColor(const Vector4& color);
    void SetColors(const Vector4& startColor, const Vector4& endColor);
    void SetScaleRange(const Vector2& rangeX, const Vector2& rangeY);
    void SetScaleRangeX(const Vector2& range);
    void SetScaleRangeY(const Vector2& range);
    void SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ);
    void SetVelRangeX(const Vector2& range);
    void SetVelRangeY(const Vector2& range);
    void SetVelRangeZ(const Vector2& range);
    void SetSpeedRange(const Vector2& range);
    void SetLifeTimeRange(const Vector2& range);

    /// <summary>
    /// 速度減衰係数を設定（per-emitter）
    /// </summary>
    /// <param name="damping">減衰係数（0.9-1.0、0.99推奨）</param>
    void SetDamping(float damping);

    /// <summary>
    /// 反発係数を設定（per-emitter）
    /// </summary>
    /// <param name="restitution">反発係数（0.0-1.0）</param>
    void SetCollisionRestitution(float restitution);

    /// <summary>
    /// 衝突判定半径を設定（per-emitter）
    /// </summary>
    /// <param name="radius">パーティクルの衝突判定半径</param>
    void SetParticleRadius(float radius);

    /// <summary>
    /// Curl Noise の空間スケールを設定（per-emitter）
    /// </summary>
    /// <param name="scale">空間スケール（小=大渦、大=細密）</param>
    void SetNoiseScale(float scale);

    /// <summary>
    /// Curl Noise の強度を設定（per-emitter）
    /// </summary>
    /// <param name="strength">強度（0.01-0.1=控えめ、0.5+=強い乱流）</param>
    void SetNoiseStrength(float strength);

    /// <summary>
    /// スポーン位置種別を設定 (中/外/線)
    /// </summary>
    /// <param name="location">SpawnLocation::Inside / Surface / Edge</param>
    /// <remarks>
    /// 形状ごとの対応:
    ///  - Sphere: Inside / Surface (Edge は Surface へフォールバック)
    ///  - Box: 全 3 種
    ///  - Triangle: Surface / Edge (Inside は Surface へフォールバック)
    ///  - Mesh: 全 3 種 (Inside は SDF を要求)
    /// </remarks>
    void SetSpawnLocation(SpawnLocation location);

    /// <summary>
    /// Per-Particle Spawn 拘束を有効化／無効化
    /// </summary>
    /// <param name="enable">有効化する場合 true</param>
    /// <param name="stiffness">バネ係数 k (粒子ごとに targetLocal へ引き寄せる力)</param>
    /// <param name="damping">ダンパ係数 d</param>
    /// <remarks>
    /// Mesh エミッタと組み合わせると「粒子群がメッシュ表面に拘束されて形状を成す」演出になる。
    /// Mesh の動的回転・移動にも追従する (meshWorld 経由)。
    /// </remarks>
    void SetSpawnLock(bool enable, float stiffness, float damping);

    /// <summary>
    /// Per-Emitter Target 収束を有効化／無効化
    /// </summary>
    /// <param name="enable">有効化する場合 true</param>
    /// <remarks>
    /// 有効時は所属パーティクル全員が targetPosition へバネ-ダンパで引き寄せられる。
    /// 動く目標を追従させたい場合は <c>BindTargetPosition()</c> でポインタを渡すか、
    /// 静的目標なら <c>SetTargetPosition()</c> を使う。
    /// </remarks>
    void SetConvergeToTarget(bool enable);

    /// <summary>
    /// 収束目標座標を静的に設定
    /// </summary>
    /// <param name="position">目標ワールド座標</param>
    /// <remarks>
    /// <c>BindTargetPosition()</c> で動的バインドされている場合は次の UpdateEmission で上書きされる。
    /// </remarks>
    void SetTargetPosition(const Vector3& position);

    /// <summary>
    /// 収束のバネ係数とダンパ係数を設定
    /// </summary>
    /// <param name="stiffness">バネ係数 k (大きいほど強く引き寄せる)</param>
    /// <param name="damping">ダンパ係数 d (大きいほど振動を抑える)</param>
    void SetConvergeParameters(float stiffness, float damping);

    /// <summary>
    /// アルファフェードを有効化／無効化
    /// </summary>
    /// <param name="enable">有効化する場合 true (既定 ON)</param>
    /// <remarks>
    /// ON: 寿命進行で alpha が 1.0 → 0.0 に線形補間 (旧挙動)。
    /// OFF: 寿命中は alpha = 1.0 を維持し、寿命終端で即非表示。スケール縮小と組み合わせると
    /// 「縮みながら最後まで不透明」の表現が可能。
    /// </remarks>
    void SetAlphaFade(bool enable);

    /// <summary>
    /// スケール縮小消滅を有効化／無効化
    /// </summary>
    /// <param name="enable">有効化する場合 true</param>
    /// <param name="endScale">寿命終端で到達するスケール。既定 (0,0,0) で完全消失</param>
    /// <remarks>
    /// alpha フェードとは独立フラグなので、片方だけ・両方併用のいずれも可。
    /// </remarks>
    void SetScaleFade(bool enable, const Vector3& endScale = { .x = 0.0f, .y = 0.0f, .z = 0.0f });

    /// <summary>
    /// スケール縮小消滅の終端スケールのみを更新（フラグは触らない）
    /// </summary>
    /// <param name="endScale">終端スケール</param>
    void SetEndScaleDefault(const Vector3& endScale);

    /// <summary>
    /// パラメータごとのランダム化フラグをまとめて設定
    /// </summary>
    /// <param name="flags">ERAND_* のビット OR。0 にすると旧来の自動判定にフォールバック</param>
    /// <remarks>
    /// 例: <c>SetRandomFlags(ERAND_SCALE_X | ERAND_VEL_X | ERAND_VEL_Y);</c>
    /// </remarks>
    void SetRandomFlags(uint32_t flags);

    /// <summary>
    /// 一時的なエミッターとして設定
    /// </summary>
    /// <param name="isTemporary">一時的にする場合 true</param>
    /// <param name="lifeTime">エミッターの寿命（秒）</param>
    void SetTemporary(bool isTemporary, float lifeTime = 0.0f);

    /// <summary>
    /// 描画ブレンドモードを設定（加算 / スクリーン / アルファ）
    /// </summary>
    /// <param name="mode">ParticleBlendMode</param>
    void SetBlendMode(ParticleBlendMode mode) { data_.blendMode = static_cast<uint32_t>(mode); }

    /// <summary>
    /// ビルボード(カメラ追従)の ON/OFF を設定。既定 ON。
    /// OFF にするとパーティクルはワールド固定向き(particle.rotate.z でXY平面回転)で描画される。
    /// </summary>
    void SetBillboard(bool enable) { SetFlag(EFLAG_BILLBOARD, enable); }

    /// <summary>
    /// このエミッターのパーティクルが使用するテクスチャを設定する。
    /// </summary>
    /// <param name="filePath">テクスチャファイルパス (TextureManager の通常ディレクトリ基準)</param>
    void SetTexture(const std::string& filePath);

    /// <summary>
    /// パーティクルの描画モデルをファイルから設定する (デフォルトは板ポリ)。
    /// 各パーティクルがそのモデル(先頭メッシュだけ)形状で描画される。
    /// </summary>
    /// <param name="modelPath">モデルファイル名</param>
    void SetParticleModel(const std::string& modelPath);

    /// <summary>
    /// パーティクルの描画モデルを既存メッシュから設定する。
    /// mesh の寿命は呼び出し側が保証すること (例: Mesh エミッターが自身のスポーンメッシュを渡す)。
    /// </summary>
    /// <param name="mesh">描画に使うメッシュ</param>
    void SetParticleModel(Mesh* mesh);

    //============================================================
    //Getter
    //============================================================
    const EmitterData& GetData() const { return data_; }

    [[nodiscard]] SpawnLocation GetSpawnLocation() const { return static_cast<SpawnLocation>(data_.spawnLocation); }
    [[nodiscard]] bool IsSpawnLock() const { return (data_.flags & EFLAG_LOCK_TO_SPAWN) != 0; }
    [[nodiscard]] float GetLockStiffness() const { return data_.lockStiffness; }
    [[nodiscard]] float GetLockDamping() const { return data_.lockDamping; }
    [[nodiscard]] bool IsConvergeToTarget() const { return (data_.flags & EFLAG_CONVERGE_TO_TARGET) != 0; }
    [[nodiscard]] const Vector3& GetTargetPosition() const { return data_.targetPosition; }
    [[nodiscard]] float GetConvergeStiffness() const { return data_.convergeStiffness; }
    [[nodiscard]] float GetConvergeDamping() const { return data_.convergeDamping; }
    [[nodiscard]] bool IsUseAlphaFade() const { return (data_.flags & EFLAG_USE_ALPHA_FADE) != 0; }
    [[nodiscard]] bool IsUseScaleFade() const { return (data_.flags & EFLAG_USE_SCALE_FADE) != 0; }
    [[nodiscard]] const Vector3& GetEndScaleDefault() const { return data_.endScaleDefault; }
    [[nodiscard]] uint32_t GetRandomFlags() const { return data_.randomFlags; }
    [[nodiscard]] bool IsRandomEnabled(uint32_t flag) const { return (data_.randomFlags & flag) != 0u; }
    [[nodiscard]] const Vector3& GetPosition() const { return data_.position; }
    [[nodiscard]] bool IsActive() const { return (data_.flags & EFLAG_ACTIVE) != 0; }
    [[nodiscard]] bool IsEmitting() const { return (data_.flags & EFLAG_EMITTING) != 0; }
    [[nodiscard]] bool IsRandomRotateZ() const { return (data_.flags & EFLAG_RANDOM_ROTATE_Z) != 0; }
    [[nodiscard]] bool IsUseForceField() const { return (data_.flags & EFLAG_USE_FORCE_FIELD) != 0; }
    [[nodiscard]] bool IsUseCurlNoise() const { return (data_.flags & EFLAG_USE_CURL_NOISE) != 0; }
    [[nodiscard]] bool IsUseDepthCollision() const { return (data_.flags & EFLAG_USE_DEPTH_COLLISION) != 0; }
    [[nodiscard]] const Vector2& GetScaleRangeX() const { return data_.scaleRangeX; }
    [[nodiscard]] const Vector2& GetScaleRangeY() const { return data_.scaleRangeY; }
    [[nodiscard]] const Vector2& GetVelRangeX() const { return data_.velRangeX; }
    [[nodiscard]] const Vector2& GetVelRangeY() const { return data_.velRangeY; }
    [[nodiscard]] const Vector2& GetVelRangeZ() const { return data_.velRangeZ; }
    [[nodiscard]] const Vector2& GetSpeedRange() const { return data_.speedRange; }
    [[nodiscard]] const Vector2& GetLifeTimeRange() const { return data_.lifeTimeRange; }
    [[nodiscard]] const Vector4& GetStartColor() const { return data_.startColorTint; }
    [[nodiscard]] const Vector4& GetEndColor() const { return data_.endColorTint; }
    [[nodiscard]] uint32_t GetParticleCount() const { return data_.count; }
    [[nodiscard]] float GetFrequencyTime() const { return data_.frequencyTime; }
    [[nodiscard]] float GetFrequency() const { return data_.frequency; }
    [[nodiscard]] uint32_t GetEmitterId() const { return data_.emitterID; }
    [[nodiscard]] bool IsTemporary() const { return (data_.flags & EFLAG_TEMPORARY) != 0; }
    [[nodiscard]] float GetEmitterLifeTime() const { return data_.emitterLifeTime; }
    [[nodiscard]] float GetEmitterCurrentTime() const { return data_.emitterCurrentTime; }

    /// <summary>
    /// エミッターの寿命が切れたかどうかを取得
    /// </summary>
    /// <returns>寿命が切れた場合 true</returns>
    [[nodiscard]] bool IsLifeTimeExpired() const;

    [[nodiscard]] bool IsNormalize() const { return (data_.flags & EFLAG_NORMALIZE) != 0; }
    [[nodiscard]] float GetDamping() const { return data_.damping; }
    [[nodiscard]] float GetCollisionRestitution() const { return data_.collisionRestitution; }
    [[nodiscard]] float GetParticleRadius() const { return data_.particleRadius; }
    [[nodiscard]] float GetNoiseScale() const { return data_.noiseScale; }
    [[nodiscard]] float GetNoiseStrength() const { return data_.noiseStrength; }
    [[nodiscard]] ParticleBlendMode GetBlendMode() const { return static_cast<ParticleBlendMode>(data_.blendMode); }
    [[nodiscard]] bool IsBillboard() const { return (data_.flags & EFLAG_BILLBOARD) != 0; }
    [[nodiscard]] uint32_t GetTextureSrvIndex() const { return data_.textureSrvIndex; }
    [[nodiscard]] bool HasParticleModel() const { return data_.renderIndexCount != 0; }
    [[nodiscard]] const std::string& GetRenderModelPath() const { return renderModelPath_; }
    [[nodiscard]] virtual EmitterType GetType() const = 0;

    // RegisterEmitter がスロット確定時に data_.emitterID へ正式 ID を書き込むため
    friend class GPUParticle;

  protected: //メンバー関数
    /// <summary>
    /// クローン共通処理: EmitterData 全体と data_ 外メンバ(renderModelPath_)を dst へ転送する。
    /// </summary>
    /// <param name="dst">コピー先エミッター</param>
    void CopyCommonStateTo(GPUParticleEmitter& dst) const;

    /// <summary>
    /// data_.flags の指定ビットを enable に応じて設定/クリアする
    /// </summary>
    void SetFlag(uint32_t flag, bool enable) { enable ? (data_.flags |= flag) : (data_.flags &= ~flag); }

  protected: //メンバー変数
    GPUParticle* particleSystem_;  ///< GPU パーティクルシステムへの参照（パーティクル生成要求の送信先）

    EmitterData data_;  ///< エミッターの全設定データ（位置、色、速度範囲、寿命など）

    std::string renderModelPath_;  ///< 描画モデルのファイルパス (空=既定板ポリ)。JSON 永続化用。

    const Vector3* boundTargetPosition_ = nullptr;  ///< 動的バインド用 Vector3 ポインタ (非所有、毎フレーム UpdateEmission で同期)
  };

} // namespace Tako