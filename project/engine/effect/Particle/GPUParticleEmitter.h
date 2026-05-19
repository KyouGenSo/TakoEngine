#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "ParticleStruct.h"
#include <memory>

namespace Tako {

  // 前方宣言
  class GPUParticle;

  /// <summary>
  /// GPU パーティクルエミッター基底クラス
  /// 球体、箱型、三角形など各種エミッターの共通機能を提供
  /// パーティクル射出タイミング制御、色・速度・スケールの範囲指定、一時エミッター機能をサポート
  /// std::enable_shared_from_this により安全な shared_ptr の生成を実現
  /// </summary>
  class GPUParticleEmitter : public std::enable_shared_from_this<GPUParticleEmitter>
  {
  public:
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
    /// エミッターデータの参照を取得（GPU転送用）
    /// </summary>
    /// <returns>エミッターデータの const 参照</returns>
    const EmitterData& GetData() const { return data_; }

    /// <summary>
    /// エミッターの射出更新
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    /// <remarks>派生クラスが追加の同期処理を行えるよう virtual。基底実装で active/emit タイマーと Stage C 動的同期を実行する</remarks>
    virtual void UpdateEmission(float deltaTime);

    /// <summary>
    /// エミッターの位置を設定
    /// </summary>
    /// <param name="position">位置</param>
    void SetPosition(const Vector3& position);

    /// <summary>
    /// エミッターのアクティブ状態を設定
    /// </summary>
    /// <param name="isActive">アクティブにする場合 true</param>
    void SetActive(bool isActive);

    /// <summary>
    /// エミッターの射出状態を設定
    /// </summary>
    /// <param name="cond">射出を有効にする場合 true</param>
    void SetEmitting(bool cond);

    /// <summary>
    /// 速度の正規化を設定
    /// </summary>
    /// <param name="isNormalize">正規化する場合 true</param>
    void SetNormalize(bool isNormalize);

    /// <summary>
    /// ランダム Z 軸回転を設定
    /// </summary>
    /// <param name="isRandomRotateZ">ランダム回転を有効にする場合 true</param>
    void SetRandomRotateZ(bool isRandomRotateZ);

    /// <summary>
    /// フォースフィールドの影響を受けるかを設定
    /// </summary>
    /// <param name="useForceField">フォースフィールドを有効にする場合 true</param>
    void SetUseForceField(bool useForceField);

    /// <summary>
    /// Curl Noise 乱流の影響を受けるかを設定
    /// </summary>
    /// <param name="useCurlNoise">Curl Noise を有効にする場合 true</param>
    void SetUseCurlNoise(bool useCurlNoise);

    /// <summary>
    /// 深度バッファ衝突の有効/無効を設定
    /// </summary>
    /// <param name="useDepthCollision">深度衝突を有効にする場合 true</param>
    void SetUseDepthCollision(bool useDepthCollision);

    /// <summary>
    /// 1回の射出で生成するパーティクル数を設定
    /// </summary>
    /// <param name="count">パーティクル数</param>
    void SetParticleCount(uint32_t count);

    /// <summary>
    /// パーティクルの射出頻度を設定
    /// </summary>
    /// <param name="frequency">射出間隔（秒）</param>
    void SetFrequency(float frequency);

    /// <summary>
    /// 現在の頻度タイマーを設定
    /// </summary>
    /// <param name="frequencyTime">タイマー値（秒）</param>
    void SetFrequencyTime(float frequencyTime);

    /// <summary>
    /// パーティクルの開始色と終了色を同じ値に設定
    /// </summary>
    /// <param name="color">色</param>
    void SetColor(const Vector4& color);

    /// <summary>
    /// パーティクルの開始色を設定
    /// </summary>
    /// <param name="color">開始色</param>
    void SetStartColor(const Vector4& color);

    /// <summary>
    /// パーティクルの終了色を設定
    /// </summary>
    /// <param name="color">終了色</param>
    void SetEndColor(const Vector4& color);

    /// <summary>
    /// パーティクルの開始色と終了色を設定
    /// </summary>
    /// <param name="startColor">開始色</param>
    /// <param name="endColor">終了色</param>
    void SetColors(const Vector4& startColor, const Vector4& endColor);

    /// <summary>
    /// スケールの乱数範囲を設定（X・Y）
    /// </summary>
    /// <param name="rangeX">X 方向の範囲（最小値、最大値）</param>
    /// <param name="rangeY">Y 方向の範囲（最小値、最大値）</param>
    void SetScaleRange(const Vector2& rangeX, const Vector2& rangeY);

    /// <summary>
    /// スケールの乱数範囲を設定（X 方向のみ）
    /// </summary>
    /// <param name="range">範囲（最小値、最大値）</param>
    void SetScaleRangeX(const Vector2& range);

    /// <summary>
    /// スケールの乱数範囲を設定（Y 方向のみ）
    /// </summary>
    /// <param name="range">範囲（最小値、最大値）</param>
    void SetScaleRangeY(const Vector2& range);

    /// <summary>
    /// 速度の乱数範囲を設定（X・Y・Z）
    /// </summary>
    /// <param name="rangeX">X 方向の範囲（最小値、最大値）</param>
    /// <param name="rangeY">Y 方向の範囲（最小値、最大値）</param>
    /// <param name="rangeZ">Z 方向の範囲（最小値、最大値）</param>
    void SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ);

    /// <summary>
    /// 速度の乱数範囲を設定（X 方向のみ）
    /// </summary>
    /// <param name="range">範囲（最小値、最大値）</param>
    void SetVelRangeX(const Vector2& range);

    /// <summary>
    /// 速度の乱数範囲を設定（Y 方向のみ）
    /// </summary>
    /// <param name="range">範囲（最小値、最大値）</param>
    void SetVelRangeY(const Vector2& range);

    /// <summary>
    /// 速度の乱数範囲を設定（Z 方向のみ）
    /// </summary>
    /// <param name="range">範囲（最小値、最大値）</param>
    void SetVelRangeZ(const Vector2& range);

    /// <summary>
    /// パーティクルの寿命範囲を設定
    /// </summary>
    /// <param name="range">寿命範囲（最小値、最大値）秒</param>
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
    /// スポーン位置種別を設定 (Stage B-2: 中/外/線)
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
    /// スポーン位置種別を取得 (Stage B-2)
    /// </summary>
    [[nodiscard]] SpawnLocation GetSpawnLocation() const { return static_cast<SpawnLocation>(data_.spawnLocation); }

    /// <summary>
    /// Per-Particle Spawn 拘束を有効化／無効化 (Stage E)
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
    /// Spawn 拘束が有効かを取得 (Stage E)
    /// </summary>
    [[nodiscard]] bool IsSpawnLock() const { return (data_.flags & EFLAG_LOCK_TO_SPAWN) != 0; }

    /// <summary>
    /// 拘束バネ係数を取得 (Stage E)
    /// </summary>
    [[nodiscard]] float GetLockStiffness() const { return data_.lockStiffness; }

    /// <summary>
    /// 拘束ダンパ係数を取得 (Stage E)
    /// </summary>
    [[nodiscard]] float GetLockDamping() const { return data_.lockDamping; }

    /// <summary>
    /// Per-Emitter Target 収束を有効化／無効化 (Stage C)
    /// </summary>
    /// <param name="enable">有効化する場合 true</param>
    /// <remarks>
    /// 有効時は所属パーティクル全員が targetPosition へバネ-ダンパで引き寄せられる。
    /// 動く目標を追従させたい場合は <c>BindTargetPosition()</c> でポインタを渡すか、
    /// 静的目標なら <c>SetTargetPosition()</c> を使う。
    /// </remarks>
    void SetConvergeToTarget(bool enable);

    /// <summary>
    /// 収束目標座標を静的に設定 (Stage C)
    /// </summary>
    /// <param name="position">目標ワールド座標</param>
    /// <remarks>
    /// <c>BindTargetPosition()</c> で動的バインドされている場合は次の UpdateEmission で上書きされる。
    /// </remarks>
    void SetTargetPosition(const Vector3& position);

    /// <summary>
    /// 収束目標座標を動的にバインド (Stage C)
    /// </summary>
    /// <param name="pPosition">毎フレーム読み取られる Vector3 へのポインタ。ライフタイム管理は呼び出し側責務</param>
    /// <remarks>
    /// 非 nullptr のとき、<c>UpdateEmission()</c> 内で毎フレーム <c>*pPosition</c> を <c>data_.targetPosition</c> に同期する。
    /// 解除するには <c>UnbindTargetPosition()</c> または <c>BindTargetPosition(nullptr)</c>。
    /// </remarks>
    void BindTargetPosition(const Vector3* pPosition);

    /// <summary>
    /// 動的バインドを解除 (Stage C)
    /// </summary>
    void UnbindTargetPosition() { boundTargetPosition_ = nullptr; }

    /// <summary>
    /// 収束のバネ係数とダンパ係数を設定 (Stage C)
    /// </summary>
    /// <param name="stiffness">バネ係数 k (大きいほど強く引き寄せる)</param>
    /// <param name="damping">ダンパ係数 d (大きいほど振動を抑える)</param>
    void SetConvergeParameters(float stiffness, float damping);

    /// <summary>
    /// Per-Emitter Target 収束が有効かを取得 (Stage C)
    /// </summary>
    [[nodiscard]] bool IsConvergeToTarget() const { return (data_.flags & EFLAG_CONVERGE_TO_TARGET) != 0; }

    /// <summary>
    /// 現在の収束目標座標を取得 (Stage C)
    /// </summary>
    [[nodiscard]] const Vector3& GetTargetPosition() const { return data_.targetPosition; }

    /// <summary>
    /// 収束のバネ係数を取得 (Stage C)
    /// </summary>
    [[nodiscard]] float GetConvergeStiffness() const { return data_.convergeStiffness; }

    /// <summary>
    /// 収束のダンパ係数を取得 (Stage C)
    /// </summary>
    [[nodiscard]] float GetConvergeDamping() const { return data_.convergeDamping; }

    /// <summary>
    /// アルファフェードを有効化／無効化 (Stage B-1 補完)
    /// </summary>
    /// <param name="enable">有効化する場合 true (既定 ON)</param>
    /// <remarks>
    /// ON: 寿命進行で alpha が 1.0 → 0.0 に線形補間 (旧挙動)。
    /// OFF: 寿命中は alpha = 1.0 を維持し、寿命終端で即非表示。スケール縮小と組み合わせると
    /// 「縮みながら最後まで不透明」の表現が可能。
    /// </remarks>
    void SetAlphaFade(bool enable);

    /// <summary>
    /// アルファフェードが有効かを取得
    /// </summary>
    [[nodiscard]] bool IsUseAlphaFade() const { return (data_.flags & EFLAG_USE_ALPHA_FADE) != 0; }

    /// <summary>
    /// スケール縮小消滅を有効化／無効化 (Stage B-1)
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
    /// スケール縮小消滅が有効かを取得
    /// </summary>
    [[nodiscard]] bool IsUseScaleFade() const { return (data_.flags & EFLAG_USE_SCALE_FADE) != 0; }

    /// <summary>
    /// スケール縮小消滅の終端スケールを取得
    /// </summary>
    [[nodiscard]] const Vector3& GetEndScaleDefault() const { return data_.endScaleDefault; }

    /// <summary>
    /// パラメータごとのランダム化フラグをまとめて設定
    /// </summary>
    /// <param name="flags">ERAND_* のビット OR。0 にすると旧来の自動判定にフォールバック</param>
    /// <remarks>
    /// 例: <c>SetRandomFlags(ERAND_SCALE_X | ERAND_VEL_X | ERAND_VEL_Y);</c>
    /// </remarks>
    void SetRandomFlags(uint32_t flags);

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
    /// パラメータごとのランダム化フラグを取得
    /// </summary>
    /// <returns>現在の ERAND_* ビット集合</returns>
    [[nodiscard]] uint32_t GetRandomFlags() const { return data_.randomFlags; }

    /// <summary>
    /// 指定 ERAND_* フラグが立っているかを判定
    /// </summary>
    /// <param name="flag">問い合わせたい単一フラグ</param>
    /// <returns>立っていれば true</returns>
    [[nodiscard]] bool IsRandomEnabled(uint32_t flag) const { return (data_.randomFlags & flag) != 0u; }

    /// <summary>
    /// 一時的なエミッターとして設定
    /// </summary>
    /// <param name="isTemporary">一時的にする場合 true</param>
    /// <param name="lifeTime">エミッターの寿命（秒）</param>
    void SetTemporary(bool isTemporary, float lifeTime = 0.0f);

    /// <summary>
    /// 一時的なエミッターの寿命を更新
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateTemporaryLifeTime(float deltaTime);

    /// <summary>
    /// エミッターの位置を取得
    /// </summary>
    /// <returns>位置</returns>
    [[nodiscard]] const Vector3& GetPosition() const { return data_.position; }

    /// <summary>
    /// エミッターがアクティブかどうかを取得
    /// </summary>
    /// <returns>アクティブな場合 true</returns>
    [[nodiscard]] bool IsActive() const { return (data_.flags & EFLAG_ACTIVE) != 0; }

    /// <summary>
    /// エミッターが射出中かどうかを取得
    /// </summary>
    /// <returns>射出中の場合 true</returns>
    [[nodiscard]] bool IsEmitting() const { return (data_.flags & EFLAG_EMITTING) != 0; }

    /// <summary>
    /// ランダム Z 軸回転が有効かどうかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsRandomRotateZ() const { return (data_.flags & EFLAG_RANDOM_ROTATE_Z) != 0; }

    /// <summary>
    /// フォースフィールドの影響を受けるかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsUseForceField() const { return (data_.flags & EFLAG_USE_FORCE_FIELD) != 0; }

    /// <summary>
    /// Curl Noise 乱流の影響を受けるかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsUseCurlNoise() const { return (data_.flags & EFLAG_USE_CURL_NOISE) != 0; }

    /// <summary>
    /// 深度バッファ衝突が有効かどうかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsUseDepthCollision() const { return (data_.flags & EFLAG_USE_DEPTH_COLLISION) != 0; }

    /// <summary>
    /// X 方向のスケール範囲を取得
    /// </summary>
    /// <returns>スケール範囲</returns>
    [[nodiscard]] const Vector2& GetScaleRangeX() const { return data_.scaleRangeX; }

    /// <summary>
    /// Y 方向のスケール範囲を取得
    /// </summary>
    /// <returns>スケール範囲</returns>
    [[nodiscard]] const Vector2& GetScaleRangeY() const { return data_.scaleRangeY; }

    /// <summary>
    /// X 方向の速度範囲を取得
    /// </summary>
    /// <returns>速度範囲</returns>
    [[nodiscard]] const Vector2& GetVelRangeX() const { return data_.velRangeX; }

    /// <summary>
    /// Y 方向の速度範囲を取得
    /// </summary>
    /// <returns>速度範囲</returns>
    [[nodiscard]] const Vector2& GetVelRangeY() const { return data_.velRangeY; }

    /// <summary>
    /// Z 方向の速度範囲を取得
    /// </summary>
    /// <returns>速度範囲</returns>
    [[nodiscard]] const Vector2& GetVelRangeZ() const { return data_.velRangeZ; }

    /// <summary>
    /// パーティクルの寿命範囲を取得
    /// </summary>
    /// <returns>寿命範囲（秒）</returns>
    [[nodiscard]] const Vector2& GetLifeTimeRange() const { return data_.lifeTimeRange; }

    /// <summary>
    /// パーティクルの開始色を取得
    /// </summary>
    /// <returns>開始色</returns>
    [[nodiscard]] const Vector4& GetStartColor() const { return data_.startColorTint; }

    /// <summary>
    /// パーティクルの終了色を取得
    /// </summary>
    /// <returns>終了色</returns>
    [[nodiscard]] const Vector4& GetEndColor() const { return data_.endColorTint; }

    /// <summary>
    /// 1回の射出で生成するパーティクル数を取得
    /// </summary>
    /// <returns>パーティクル数</returns>
    [[nodiscard]] uint32_t GetParticleCount() const { return data_.count; }

    /// <summary>
    /// 現在の頻度タイマー値を取得
    /// </summary>
    /// <returns>タイマー値（秒）</returns>
    [[nodiscard]] float GetFrequencyTime() const { return data_.frequencyTime; }

    /// <summary>
    /// パーティクルの射出頻度を取得
    /// </summary>
    /// <returns>射出間隔（秒）</returns>
    [[nodiscard]] float GetFrequency() const { return data_.frequency; }

    /// <summary>
    /// エミッター ID を取得
    /// </summary>
    /// <returns>エミッター ID</returns>
    [[nodiscard]] uint32_t GetEmitterId() const { return data_.emitterID; }

    /// <summary>
    /// 一時的なエミッターかどうかを取得
    /// </summary>
    /// <returns>一時的な場合 true</returns>
    [[nodiscard]] bool IsTemporary() const { return (data_.flags & EFLAG_TEMPORARY) != 0; }

    /// <summary>
    /// エミッターの寿命を取得
    /// </summary>
    /// <returns>寿命（秒）</returns>
    [[nodiscard]] float GetEmitterLifeTime() const { return data_.emitterLifeTime; }

    /// <summary>
    /// エミッターの現在の経過時間を取得
    /// </summary>
    /// <returns>経過時間（秒）</returns>
    [[nodiscard]] float GetEmitterCurrentTime() const { return data_.emitterCurrentTime; }

    /// <summary>
    /// エミッターの寿命が切れたかどうかを取得
    /// </summary>
    /// <returns>寿命が切れた場合 true</returns>
    [[nodiscard]] bool IsLifeTimeExpired() const;

    /// <summary>
    /// 速度の正規化が有効かどうかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsNormalize() const { return (data_.flags & EFLAG_NORMALIZE) != 0; }

    /// <summary>
    /// 速度減衰係数を取得（per-emitter）
    /// </summary>
    [[nodiscard]] float GetDamping() const { return data_.damping; }

    /// <summary>
    /// 反発係数を取得（per-emitter）
    /// </summary>
    [[nodiscard]] float GetCollisionRestitution() const { return data_.collisionRestitution; }

    /// <summary>
    /// 衝突判定半径を取得（per-emitter）
    /// </summary>
    [[nodiscard]] float GetParticleRadius() const { return data_.particleRadius; }

    /// <summary>
    /// Curl Noise の空間スケールを取得（per-emitter）
    /// </summary>
    [[nodiscard]] float GetNoiseScale() const { return data_.noiseScale; }

    /// <summary>
    /// Curl Noise の強度を取得（per-emitter）
    /// </summary>
    [[nodiscard]] float GetNoiseStrength() const { return data_.noiseStrength; }


    /// <summary>
    /// エミッタータイプを取得（派生クラスで実装）
    /// </summary>
    /// <returns>エミッタータイプ</returns>
    [[nodiscard]] virtual EmitterType GetType() const = 0;

  protected:
    GPUParticle* particleSystem_;    ///< GPU パーティクルシステムへの参照（パーティクル生成要求の送信先）

    EmitterData data_;               ///< エミッターの全設定データ（位置、色、速度範囲、寿命など）

    const Vector3* boundTargetPosition_ = nullptr; ///< Stage C: 動的バインド用 Vector3 ポインタ (非所有、毎フレーム UpdateEmission で同期)
  };

} // namespace Tako