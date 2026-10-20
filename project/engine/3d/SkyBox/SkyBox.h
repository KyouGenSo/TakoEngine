#pragma once
#include <d3d12.h>
#include <string>
#include <wrl/client.h>

#include "Matrix4x4.h"
#include "Material.h"
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

  struct MaterialConstants
  {
    Vector4 color;
  };

  struct TransformationMatrix
  {
    Matrix4x4 world;  ///< VP は描画時にバインドされた視点 CB 側で掛ける
  };

public: // メンバー関数

  /// <summary>
  /// 指定キューブマップテクスチャでスカイボックスを初期化
  /// </summary>
  /// <param name="texturePath">キューブマップテクスチャのパス</param>
  void Initialize(const std::string& texturePath);
  void Update();

  /// <summary>
  /// 描画リストへ登録する
  /// </summary>
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
    material_->SetTexture("gSkyBoxTexture"_id, textureIndex_);
  }

  //===========================================
  //Getter
  //===========================================
  uint32_t GetTextureIndex() const { return textureIndex_; }

private: // プライベートメンバー関数
  void CreateVertexData();
  void CreateIndexData();
  void CreateMaterialData();
  void CreateTransformationMatrixData();

private: // メンバ変数
  Transform transform_ = {};

  Matrix4x4 worldMatrix_ = {};

  DX12Basic* dx12_ = nullptr;

  std::unique_ptr<Material> material_;

  //バッファリソース
  ComPtr<ID3D12Resource> vertexResource_;
  ComPtr<ID3D12Resource> indexResource_;
  ComPtr<ID3D12Resource> transformationMatrixResource_;

  //各バッファのマップ済みポインタ
  VertexData*           vertexData_               = nullptr;
  uint32_t*             indexData_                = nullptr;
  MaterialConstants*    materialData_             = nullptr;
  TransformationMatrix* transformationMatrixData_ = nullptr;

  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

  D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};

  uint32_t textureIndex_ = 0;
};

} // namespace Tako

