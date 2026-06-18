#pragma once
#include <d3d12.h>
#include <string>
#include <wrl/client.h>

#include "Matrix4x4.h"
#include "TextureManager.h"
#include "Transform.h"
#include "Vector2.h"
#include "Vector4.h"

namespace Tako {

// 前方宣言
class DX12Basic;

/// <summary>
/// キューブマップテクスチャを使用したスカイボックス描画クラス
/// 環境マッピングやシーンの背景として使用される
/// </summary>
class SkyBox
{
public: // 構造体
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  struct VertexData
  {
    Vector4 position;
  };

  struct Material
  {
    Vector4 color;
  };

  struct TransformationMatrix
  {
    Matrix4x4 WVP;
  };

public: // メンバー関数

  /// <summary>
  /// 指定キューブマップテクスチャでスカイボックスを初期化
  /// </summary>
  /// <param name="texturePath">キューブマップテクスチャのパス</param>
  void Initialize(const std::string& texturePath);
  void Update();
  void Draw();

  //===========================================
  //Setter
  //===========================================
  void SetScale(const Vector3& scale) { transform_.scale = scale; }
  void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
  void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

  /// <summary>
  /// テクスチャを差し替え（TextureManager から SRV インデックスを取得して保持）
  /// </summary>
  void SetTexture(const std::string& texturePath) {
    textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath);
  }

  //===========================================
  //Getter
  //===========================================
  uint32_t GetTextureIndex() const { return textureIndex_; }

private: // プライベートメンバー関数
  void CreateRootSignature();
  void CreatePSO();
  void CreateVertexData();
  void CreateIndexData();
  void CreateMaterialData();
  void CreateTransformationMatrixData();

private: // メンバ変数
  Transform transform_ = {};

  Matrix4x4 viewProjectionMatrix_ = {};
  Matrix4x4 worldMatrix_          = {};
  Matrix4x4 wvpMatrix_            = {};

  DX12Basic* m_dx12_ = nullptr;

  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

  //バッファリソース
  ComPtr<ID3D12Resource> vertexResource_;
  ComPtr<ID3D12Resource> indexResource_;
  ComPtr<ID3D12Resource> materialResource_;
  ComPtr<ID3D12Resource> transformationMatrixResource_;

  //各バッファのマップ済みポインタ
  VertexData*           vertexData_               = nullptr;
  uint32_t*             indexData_                = nullptr;
  Material*             materialData_             = nullptr;
  TransformationMatrix* transformationMatrixData_ = nullptr;

  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

  D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

  uint32_t textureIndex_ = 0;
};

} // namespace Tako

