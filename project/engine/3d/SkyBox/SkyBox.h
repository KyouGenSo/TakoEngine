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
  // 頂点データ
  struct VertexData
  {
    Vector4 position;
  };

  // マテリアルデータ
  struct Material
  {
    Vector4 color;
  };

  // 座標変換行列データ
  struct TransformationMatrix
  {
    Matrix4x4 WVP;
  };

public: // メンバ変数
  // ComPtrのエイリアス
  template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // 初期化
  void Initialize(const std::string& texturePath);
  // 更新
  void Update();
  // 描画
  void Draw();

  //---------------------Setter---------------------//
  void SetScale(const Vector3& scale) { transform_.scale = scale; }
  void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
  void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
  void SetTexture(const std::string& texturePath) {
    textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath);
  }

  //---------------------Getter---------------------//
  uint32_t GetTextureIndex() const { return textureIndex_; }

private: // プライベートメンバー関数
  // RootSignatureを生成
  void CreateRootSignature();
  // パイプラインステートを生成
  void CreatePSO();
  // 頂点データを生成
  void CreateVertexData();
  // インデックスデータを生成
  void CreateIndexData();
  // マテリアルデータを生成
  void CreateMaterialData();
  // 座標変換行列データを生成
  void CreateTransformationMatrixData();

private: // メンバ変数
  // Transform
  Transform transform_ = {};

  // 座標変換行列
  Matrix4x4 viewProjectionMatrix_ = {};
  Matrix4x4 worldMatrix_ = {};
  Matrix4x4 wvpMatrix_ = {};

  DX12Basic* m_dx12_ = nullptr; // DX12の基本情報

  // ルートシグネチャ
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

  // パイプラインステート
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

  // バッファリソース
  ComPtr<ID3D12Resource> vertexResource_;
  ComPtr<ID3D12Resource> indexResource_;
  ComPtr<ID3D12Resource> materialResource_;
  ComPtr<ID3D12Resource> transformationMatrixResource_;

  // バッファリソース内のデータを参照するためのポインタ
  VertexData* vertexData_ = nullptr;
  uint32_t* indexData_ = nullptr;
  Material* materialData_ = nullptr;
  TransformationMatrix* transformationMatrixData_ = nullptr;

  // 頂点バッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

  // インデックスバッファビュー
  D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

  // テクスチャ番号
  uint32_t textureIndex_ = 0;
};

} // namespace Tako

