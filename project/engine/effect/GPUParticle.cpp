#include "GPUParticle.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "TextureManager.h"

GPUParticle* GPUParticle::instance_ = nullptr;

GPUParticle* GPUParticle::GetInstance()
{
  if (instance_ == nullptr)
  {
    instance_ = new GPUParticle();
  }
  return instance_;
}

void GPUParticle::Initialize(DX12Basic* dx12, Camera* camera)
{
  m_dx12_ = dx12;

  // カメラの設定
  m_camera_ = camera;

  m_srvManager_ = SrvManager::GetInstance();

  modelData_.textureData.texturePath = "circle.png";
  modelData_.textureData.textureIndex = TextureManager::GetInstance()->GetSRVIndex(modelData_.textureData.texturePath);

  // PSOの生成
  CreatePSO();
  CreateInitPSO();

  // 頂点データの生成
  CreateVertexData();

  // マテリアルデータの生成
  CreateMaterialData();
}

void GPUParticle::Update()
{
}

void GPUParticle::Draw()
{
}

void GPUParticle::Finalize()
{
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

//--------------------------------------Private--------------------------------------//
void GPUParticle::CreateRS()
{
}

void GPUParticle::CreatePSO()
{
  CreateRS();
}

void GPUParticle::CreateInitRS()
{
}

void GPUParticle::CreateInitPSO()
{
}

void GPUParticle::CreateVertexData()
{
  modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
  modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
  modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
  modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
  modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
  modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });

  // 頂点リソース生成
  vertexResource_ = m_dx12_->MakeBufferResource(sizeof(VertexData) * modelData_.vertices.size());

  // VertexBufferViewの作成
  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();				// リソースの先頭のアドレスから使う
  vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());	// 使用するリソースのサイズは頂点のサイズ
  vertexBufferView_.StrideInBytes = sizeof(VertexData);									// 1頂点あたりのサイズ

  // 頂点リソースをマップ
  vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
  // 頂点データをリソースにコピー
  std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void GPUParticle::CreateMaterialData()
{
  // マテリアルリソース生成
  materialResource_ = m_dx12_->MakeBufferResource(sizeof(Material));

  // マテリアルリソースをマップ
  materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&material_));

  // マテリアルデータの初期値を設定
  material_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
  material_->uvTransform = Mat4x4::MakeIdentity();
}

void GPUParticle::CreateParticleResourceForCS()
{
  // ParticleCSのリソースを生成
  m_dx12_->CreateResourceForUAV(particleResourceForCS_, sizeof(ParticleCS) * kNumMaxInstance_);

  // ParticleCSのUAVを生成
  initParticleCSUavIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateUAV(initParticleCSUavIndex_, particleResourceForCS_.Get(), kNumMaxInstance_, sizeof(ParticleCS));

  // ParticleCSのSRVを生成
  initParticleCSSrvIndex_ = m_srvManager_->Allocate();
  m_srvManager_->CreateSRVForStructuredBuffer(initParticleCSSrvIndex_, particleResourceForCS_.Get(), kNumMaxInstance_, sizeof(ParticleCS));
}
