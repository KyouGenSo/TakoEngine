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
    /// GPU データの設定
    /// </summary>
    /// <param name="gpuData">設定する GPU データ</param>
    virtual void SetupGPUData(EmitterGPUData& gpuData) const;

    /// <summary>
    /// エミッターの射出更新
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void UpdateEmission(float deltaTime);

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
    [[nodiscard]] bool IsActive() const { return data_.isActive; }

    /// <summary>
    /// エミッターが射出中かどうかを取得
    /// </summary>
    /// <returns>射出中の場合 true</returns>
    [[nodiscard]] bool IsEmitting() const { return data_.isEmitting; }

    /// <summary>
    /// ランダム Z 軸回転が有効かどうかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsRandomRotateZ() const { return data_.isRandomRotateZ; }

    /// <summary>
    /// フォースフィールドの影響を受けるかを取得
    /// </summary>
    /// <returns>有効な場合 true</returns>
    [[nodiscard]] bool IsUseForceField() const { return data_.useForceField; }

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
    [[nodiscard]] bool IsTemporary() const { return data_.isTemp; }

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
    [[nodiscard]] bool IsNormalize() const { return data_.isNormalize; }


    /// <summary>
    /// エミッタータイプを取得（派生クラスで実装）
    /// </summary>
    /// <returns>エミッタータイプ</returns>
    [[nodiscard]] virtual EmitterType GetType() const = 0;

  protected:
    GPUParticle* particleSystem_;    ///< GPU パーティクルシステムへの参照（パーティクル生成要求の送信先）

    EmitterData data_;               ///< エミッターの全設定データ（位置、色、速度範囲、寿命など）
  };

} // namespace Tako