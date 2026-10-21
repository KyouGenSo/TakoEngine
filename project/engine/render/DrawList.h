#pragma once
#include <cstdint>
#include <d3d12.h>
#include <span>
#include <vector>

#include "Vector3.h"

namespace Tako {

  class Material;

  /// <summary>
  /// 描画物 1 つ分。ジオメトリ・マテリアル・描画物ごとの値を持ち、描画はパスを回すときにまとめて行う
  /// </summary>
  struct DrawItem {
    //描き方
    const Material* material = nullptr;

    //ジオメトリ（フレーム中は参照先が生きていること）
    const D3D12_VERTEX_BUFFER_VIEW* vertexBuffer  = nullptr;
    const D3D12_INDEX_BUFFER_VIEW*  indexBuffer   = nullptr;
    uint32_t                        indexCount    = 0;
    uint32_t                        startIndex    = 0;
    uint32_t                        instanceCount = 1;

    //描画物ごとの値
    D3D12_GPU_VIRTUAL_ADDRESS transform       = 0;  ///< gTransformationMatrix（0 なら渡さない）
    uint32_t                  instanceSrv     = 0;  ///< 0 以外ならインスタンシング版で描き、gInstanceData に渡す
    uint32_t                  textureOverride = 0;  ///< 0 以外ならマテリアルの gTexture の代わりに渡す

    //並び順・パスへの参加
    Vector3 sortPosition{};       ///< 奥から手前へ並べるときのワールド座標
    int32_t sortingOrder = 0;     ///< 同じ描画キューの中の順番（小さいほど先）
    bool    depthSort    = true;  ///< false なら登録順を保つ（スクリーン空間の描画物）
    bool    transparent  = false; ///< マテリアルに関わらず両面・深度書き込みなし・半透明キュー以降で描く
    bool    castShadow   = true;  ///< シャドウマップへ描くか（シェーダーが ShadowCaster パスを持つ場合）

    //オブジェクト単位のポストエフェクト・描画パスの絞り込み
    uint32_t maskId = 0;  ///< 0 以外なら Mask パスで ObjectMask に書く（1〜255）
    uint32_t nameId = 0;  ///< Object3d の名前の FNV-1a（0 は名前なし）
  };

  /// <summary>
  /// 1 フレームに登録された描画物の一覧
  /// </summary>
  class DrawList {
  public: //メンバー関数
    void Add(const DrawItem& item) { items_.push_back(item); }

    void Clear() { items_.clear(); }

    //=========================================
    //Getter
    //=========================================
    std::span<const DrawItem> GetItems() const { return items_; }

  private: //メンバー変数
    std::vector<DrawItem> items_;
  };

} // namespace Tako
