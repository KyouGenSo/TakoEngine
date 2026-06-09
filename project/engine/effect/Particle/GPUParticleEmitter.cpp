#include "GPUParticleEmitter.h"
#include "GPUParticle.h"
#include "TextureManager.h"
#include "Mesh.h"

namespace Tako {

  GPUParticleEmitter::GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId)
    : particleSystem_(particleSystem)
  {
    data_.emitterID = emitterId;
    data_.type = static_cast<uint32_t>(EmitterType::Sphere);
    data_.flags = EFLAG_ACTIVE | EFLAG_USE_FORCE_FIELD | EFLAG_USE_ALPHA_FADE | EFLAG_BILLBOARD;
    data_.emitterLifeTime = 0.0f;
    data_.emitterCurrentTime = 0.0f;
    data_.frequencyTime = 0.0f;
    data_.position = Vector3(0.0f, 0.0f, 0.0f);
    data_.scaleRangeX = Vector2(0.0f, 0.0f);
    data_.scaleRangeY = Vector2(0.0f, 0.0f);
    data_.velRangeX = Vector2(0.0f, 0.0f);
    data_.velRangeY = Vector2(0.0f, 0.0f);
    data_.velRangeZ = Vector2(0.0f, 0.0f);
    data_.lifeTimeRange = Vector2(0.0f, 0.0f);
    data_.startColorTint = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    data_.endColorTint = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    data_.count = 0;
    data_.frequency = 0.0f;
    data_.radius = 1.0f;
    data_.randomFlags = 0; // 0 = 旧来の自動判定にフォールバック（既存 JSON プリセットの後方互換）
  }

  GPUParticleEmitter::~GPUParticleEmitter()
  {
    // デストラクタで特に何もしない
    // 必要なリソースは GPUParticle クラスが管理しているため
    // ここで解放する必要はない
  }

  void GPUParticleEmitter::UpdateEmission(float deltaTime)
  {
    // 動的バインドされた目標座標を毎フレーム同期 (active/inactive に関わらず実行)
    if (boundTargetPosition_) {
      data_.targetPosition = *boundTargetPosition_;
    }

    // 非アクティブならスキップ
    if (!(data_.flags & EFLAG_ACTIVE)) {
      data_.flags &= ~EFLAG_EMITTING;
      return;
    }

    // 射出タイマーを更新
    data_.frequencyTime += deltaTime;

    // 射出間隔を超えたら射出許可を出して時間を調整
    if (data_.frequency <= data_.frequencyTime) {
      data_.flags |= EFLAG_EMITTING;

      // 余剰時間を調整（蓄積誤差を防ぐ）
      data_.frequencyTime = fmodf(data_.frequencyTime, data_.frequency);
    }
    else {
      data_.flags &= ~EFLAG_EMITTING;
    }
  }

  void GPUParticleEmitter::SetPosition(const Vector3& position)
  {
    data_.position = position;
  }

  void GPUParticleEmitter::SetActive(bool isActive)
  {
    if (isActive) data_.flags |= EFLAG_ACTIVE; else data_.flags &= ~EFLAG_ACTIVE;
  }

  void GPUParticleEmitter::SetEmitting(bool cond)
  {
    if (cond) data_.flags |= EFLAG_EMITTING; else data_.flags &= ~EFLAG_EMITTING;
  }

  void GPUParticleEmitter::SetNormalize(bool isNormalize)
  {
    if (isNormalize) data_.flags |= EFLAG_NORMALIZE; else data_.flags &= ~EFLAG_NORMALIZE;
  }

  void GPUParticleEmitter::SetRandomRotateZ(bool isRandomRotateZ)
  {
    if (isRandomRotateZ) data_.flags |= EFLAG_RANDOM_ROTATE_Z; else data_.flags &= ~EFLAG_RANDOM_ROTATE_Z;
  }

  void GPUParticleEmitter::SetUseForceField(bool useForceField)
  {
    if (useForceField) data_.flags |= EFLAG_USE_FORCE_FIELD; else data_.flags &= ~EFLAG_USE_FORCE_FIELD;
  }

  void GPUParticleEmitter::SetUseCurlNoise(bool useCurlNoise)
  {
    if (useCurlNoise) data_.flags |= EFLAG_USE_CURL_NOISE; else data_.flags &= ~EFLAG_USE_CURL_NOISE;
  }

  void GPUParticleEmitter::SetUseDepthCollision(bool useDepthCollision)
  {
    if (useDepthCollision) data_.flags |= EFLAG_USE_DEPTH_COLLISION; else data_.flags &= ~EFLAG_USE_DEPTH_COLLISION;
  }

  void GPUParticleEmitter::SetColor(const Vector4& color)
  {
    SetColors(color, color);
  }

  void GPUParticleEmitter::SetStartColor(const Vector4& color)
  {
    data_.startColorTint = color;
  }

  void GPUParticleEmitter::SetEndColor(const Vector4& color)
  {
    data_.endColorTint = color;
  }

  void GPUParticleEmitter::SetColors(const Vector4& startColor, const Vector4& endColor)
  {
    data_.startColorTint = startColor;
    data_.endColorTint = endColor;
  }

  void GPUParticleEmitter::SetParticleCount(uint32_t count)
  {
    data_.count = count;
  }

  void GPUParticleEmitter::SetFrequency(float frequency)
  {
    data_.frequency = frequency;
  }

  void GPUParticleEmitter::SetFrequencyTime(float frequencyTime)
  {
    data_.frequencyTime = frequencyTime;
  }

  void GPUParticleEmitter::SetScaleRange(const Vector2& rangeX, const Vector2& rangeY)
  {
    SetScaleRangeX(rangeX);
    SetScaleRangeY(rangeY);
  }

  void GPUParticleEmitter::SetScaleRangeX(const Vector2& range)
  {
    data_.scaleRangeX = range;
  }

  void GPUParticleEmitter::SetScaleRangeY(const Vector2& range)
  {
    data_.scaleRangeY = range;
  }

  void GPUParticleEmitter::SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ)
  {
    SetVelRangeX(rangeX);
    SetVelRangeY(rangeY);
    SetVelRangeZ(rangeZ);
  }

  void GPUParticleEmitter::SetVelRangeX(const Vector2& range)
  {
    data_.velRangeX = range;
  }

  void GPUParticleEmitter::SetVelRangeY(const Vector2& range)
  {
    data_.velRangeY = range;
  }

  void GPUParticleEmitter::SetVelRangeZ(const Vector2& range)
  {
    data_.velRangeZ = range;
  }

  void GPUParticleEmitter::SetLifeTimeRange(const Vector2& range)
  {
    data_.lifeTimeRange = range;
  }

  void GPUParticleEmitter::SetDamping(float damping)
  {
    data_.damping = damping;
  }

  void GPUParticleEmitter::SetCollisionRestitution(float restitution)
  {
    data_.collisionRestitution = restitution;
  }

  void GPUParticleEmitter::SetParticleRadius(float radius)
  {
    data_.particleRadius = radius;
  }

  void GPUParticleEmitter::SetNoiseScale(float scale)
  {
    data_.noiseScale = scale;
  }

  void GPUParticleEmitter::SetNoiseStrength(float strength)
  {
    data_.noiseStrength = strength;
  }

  void GPUParticleEmitter::SetTexture(const std::string& filePath)
  {
    // テクスチャを読み込み、その SRV インデックスを保存する。
    // 描画ループ側で textureSrvIndex != 0 のとき per-emitter テクスチャとして使用する。
    TextureManager::GetInstance()->LoadTexture(filePath);
    data_.textureSrvIndex = TextureManager::GetInstance()->GetSRVIndex(filePath);
  }

  void GPUParticleEmitter::SetParticleModel(const std::string& modelPath)
  {
    // 描画モデルをシステムにロード・保持させ、その先頭メッシュを描画モデルに設定する。
    Mesh* mesh = particleSystem_ ? particleSystem_->AcquireModelMesh(modelPath) : nullptr;
    SetParticleModel(mesh);
    // 成功時のみパスを記録 (JSON 永続化・エディタ表示用)。SetParticleModel(Mesh*) がクリアした後に上書きする。
    if (mesh) renderModelPath_ = modelPath;
  }

  void GPUParticleEmitter::SetParticleModel(Mesh* mesh)
  {
    if (!mesh) {
      ResetParticleModel();
      return;
    }
    // 描画モデルの SRV 群を記録する 
    data_.renderVertexSrvIndex = mesh->GetVertexSrvIndex();
    data_.renderIndexSrvIndex = mesh->GetIndexSrvIndex();
    data_.renderIndexCount = mesh->GetIndexCount();
    // Mesh* 直接指定はパス不明なのでパス記録をクリア (非永続)。
    renderModelPath_.clear();
  }

  void GPUParticleEmitter::ResetParticleModel()
  {
    // 既定の板ポリ描画に戻す。
    data_.renderVertexSrvIndex = 0;
    data_.renderIndexSrvIndex = 0;
    data_.renderIndexCount = 0;
    renderModelPath_.clear();
  }

  void GPUParticleEmitter::CopyCommonStateTo(GPUParticleEmitter& dst) const
  {
    // クローン共通処理: EmitterData 全体をコピーし、
    // data_ に含まれないrenderModelPath_を明示的に転送する。
    dst.data_ = data_;
    dst.renderModelPath_ = renderModelPath_;
  }

  void GPUParticleEmitter::SetSpawnLocation(SpawnLocation location)
  {
    data_.spawnLocation = static_cast<uint32_t>(location);
  }

  void GPUParticleEmitter::SetAlphaFade(bool enable)
  {
    if (enable) data_.flags |= EFLAG_USE_ALPHA_FADE; else data_.flags &= ~EFLAG_USE_ALPHA_FADE;
  }

  void GPUParticleEmitter::SetConvergeToTarget(bool enable)
  {
    if (enable) data_.flags |= EFLAG_CONVERGE_TO_TARGET; else data_.flags &= ~EFLAG_CONVERGE_TO_TARGET;
  }

  void GPUParticleEmitter::SetSpawnLock(bool enable, float stiffness, float damping)
  {
    if (enable) data_.flags |= EFLAG_LOCK_TO_SPAWN; else data_.flags &= ~EFLAG_LOCK_TO_SPAWN;
    data_.lockStiffness = stiffness;
    data_.lockDamping = damping;
  }

  void GPUParticleEmitter::SetTargetPosition(const Vector3& position)
  {
    data_.targetPosition = position;
    // 静的指定なのでバインドは解除する
    boundTargetPosition_ = nullptr;
  }

  void GPUParticleEmitter::BindTargetPosition(const Vector3* pPosition)
  {
    boundTargetPosition_ = pPosition;
    // バインド時点で即座に一度同期 (UpdateEmission を待たずに最新値を反映)
    if (pPosition) {
      data_.targetPosition = *pPosition;
    }
  }

  void GPUParticleEmitter::SetConvergeParameters(float stiffness, float damping)
  {
    data_.convergeStiffness = stiffness;
    data_.convergeDamping = damping;
  }

  void GPUParticleEmitter::SetScaleFade(bool enable, const Vector3& endScale)
  {
    if (enable) data_.flags |= EFLAG_USE_SCALE_FADE; else data_.flags &= ~EFLAG_USE_SCALE_FADE;
    data_.endScaleDefault = endScale;
  }

  void GPUParticleEmitter::SetEndScaleDefault(const Vector3& endScale)
  {
    data_.endScaleDefault = endScale;
  }

  void GPUParticleEmitter::SetRandomFlags(uint32_t flags)
  {
    data_.randomFlags = flags;
  }

  void GPUParticleEmitter::EnableRandom(uint32_t mask)
  {
    data_.randomFlags |= mask;
  }

  void GPUParticleEmitter::DisableRandom(uint32_t mask)
  {
    data_.randomFlags &= ~mask;
  }

  void GPUParticleEmitter::SetTemporary(bool isTemporary, float lifeTime)
  {
    if (isTemporary) data_.flags |= EFLAG_TEMPORARY; else data_.flags &= ~EFLAG_TEMPORARY;
    data_.emitterLifeTime = lifeTime;
    data_.emitterCurrentTime = 0.0f;
  }

  void GPUParticleEmitter::UpdateTemporaryLifeTime(const float deltaTime)
  {
    if (!(data_.flags & EFLAG_TEMPORARY) || data_.emitterLifeTime <= 0.0f) return;

    data_.emitterCurrentTime += deltaTime;
  }

  bool GPUParticleEmitter::IsLifeTimeExpired() const
  {
    if (!(data_.flags & EFLAG_TEMPORARY) || data_.emitterLifeTime <= 0.0f) return false;
    return data_.emitterCurrentTime >= data_.emitterLifeTime;
  }

} // namespace Tako
