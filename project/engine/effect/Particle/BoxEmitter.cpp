#include "BoxEmitter.h"
#include "GPUParticle.h"

namespace Tako {

  BoxEmitter::BoxEmitter(GPUParticle* particleSystem, const Vector3& position,
    const Vector3& size, const Vector3& rotation,
    uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID を設定
  {
    // パラメータの初期化
    SetPosition(position);
    SetSize(size);
    SetRotation(rotation);
    SetParticleCount(count);
    SetFrequency(frequency);

    // エミッターのタイプを設定
    data_.type = static_cast<uint32_t>(EmitterType::Box);
  }

  std::shared_ptr<GPUParticleEmitter> BoxEmitter::Clone() const
  {
    // 新しいインスタンスを作成し、現在のデータをコピー
    auto clone = std::make_shared<BoxEmitter>(
      particleSystem_, GetPosition(), GetSize(), GetRotation(), GetParticleCount(), GetFrequency());
    // 他のプロパティも転送

    clone->SetColors(GetStartColor(), GetEndColor());
    clone->SetVelRange(GetVelRangeX(), GetVelRangeY(), GetVelRangeZ());
    clone->SetLifeTimeRange(GetLifeTimeRange());
    clone->SetScaleRange(GetScaleRangeX(), GetScaleRangeY());
    clone->SetActive(IsActive());
    clone->SetNormalize(IsNormalize());
    clone->SetRandomRotateZ(IsRandomRotateZ());
    clone->SetFrequencyTime(GetFrequency());
    CopyDrawStateTo(*clone); // ブレンド/ビルボード/テクスチャ/描画モデルを転送

    return clone;
  }

  void BoxEmitter::SetSize(const Vector3& size)
  {
    data_.boxSize = size;
  }

  void BoxEmitter::SetRotation(const Vector3& rotation)
  {
    data_.boxRotation = rotation;
  }

} // namespace Tako