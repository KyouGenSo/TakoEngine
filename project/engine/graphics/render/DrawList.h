#pragma once
#include <cstdint>
#include <d3d12.h>
#include <optional>
#include <span>
#include <vector>

#include "Frustum.h"
#include "Vector3.h"

namespace Tako {

  class Material;
  class MaterialPropertyBlock;

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
    D3D12_GPU_VIRTUAL_ADDRESS    transform       = 0;        ///< gTransformationMatrix（0 なら渡さない）
    uint32_t                     instanceSrv     = 0;        ///< 0 以外ならインスタンシング版で描き、gInstanceData に渡す
    uint32_t                     textureOverride = 0;        ///< 0 以外ならマテリアルの gTexture の代わりに渡す
    const MaterialPropertyBlock* properties      = nullptr;  ///< 描くマテリアル（パスの差し替え先も）の同じ名前の値・テクスチャに重ねる（フレーム中は参照先が生きていること）

    //並び順・パスへの参加
    Vector3 sortPosition{};       ///< 奥から手前へ並べるときのワールド座標
    int32_t sortingOrder = 0;     ///< 同じ描画キューの中の順番（小さいほど先）
    bool    depthSort    = true;  ///< false なら登録順を保つ（スクリーン空間の描画物）
    bool    transparent  = false; ///< マテリアルに関わらず両面・深度書き込みなし・半透明キュー以降で描く
    bool    castShadow   = true;  ///< シャドウマップへ描くか（シェーダーが ShadowCaster パスを持つ場合）
    bool    postEffect   = true;  ///< false なら画面全体のポストエフェクトの後に、シーンの深度で前後を判定して描く（マスク付きのエフェクトも掛からない）

    //範囲（視点ごとのカリングに使う。本編・Debug Viewport・影）
    // ponytail: 球で判定するので、細長い物は緩い（描かなくていい物を描く側に倒れる）。締めるなら箱も持たせて、面との判定を箱で行う
    Vector3 boundsCenter{};         ///< 描画物を囲む球の中心（ワールド）
    float   boundsRadius   = 0.0f;  ///< 球の半径。0 なら範囲が分からない物として、カリングせず常に描く

    //オブジェクト単位のポストエフェクト・描画パスの絞り込み
    uint32_t maskId = 0;  ///< 同じ maskId の有効なポストエフェクトがあれば、シーンとは別の RT に描いてエフェクトを掛けてから重ねる（1〜255）
    uint32_t nameId = 0;  ///< Object3d の名前の FNV-1a（0 は名前なし）
  };

  /// <summary>
  /// モデルのメッシュ 1 つを描くときに DrawItem へ入れる値（Object3d がコードの上書き・設定・既定から決める）
  /// </summary>
  struct MeshDrawSlot {
    const Material*              material   = nullptr;  ///< nullptr ならメッシュのマテリアル
    const MaterialPropertyBlock* properties = nullptr;  ///< 描画物ごとの値（nullptr なら無し）
    bool                         visible    = true;
  };

  /// <summary>
  /// Renderer::RenderList で描く描画物の絞り込みと、マテリアルの差し替え
  /// </summary>
  struct DrawFilter {
    int32_t                   minQueue         = INT32_MIN;  ///< 描画キューが [minQueue, maxQueue) の描画物だけ描く
    int32_t                   maxQueue         = INT32_MAX;
    std::span<const uint32_t> nameIds;                       ///< 空でなければ DrawItem::nameId が含まれる描画物だけ描く
    std::span<const uint32_t> excludeNameIds;                ///< DrawItem::nameId が含まれる描画物は描かない
    std::span<const uint32_t> maskIds;                       ///< 空でなければ DrawItem::maskId が含まれる描画物だけ描く
    std::span<const uint32_t> excludeMaskIds;                ///< DrawItem::maskId が含まれる描画物は描かない
    std::optional<bool>       postEffect;                    ///< 値があれば DrawItem::postEffect が一致する描画物だけ描く
    const Material*           overrideMaterial = nullptr;    ///< 全ての描画物をこのマテリアル（の描画設定）で描く
  };

  /// <summary>
  /// 1 フレームに登録された描画物の一覧
  /// </summary>
  class DrawList {
  public: //メンバー関数
    void Add(const DrawItem& item)
    {
      items_.push_back(item);
      visibilityValid_ = false;
    }

    void Clear()
    {
      items_.clear();
      visibilityValid_ = false;
    }

    //=========================================
    //Getter
    //=========================================
    std::span<const DrawItem> GetItems() const { return items_; }

    /// <summary>
    /// frustum に掛かる描画物の印（GetItems と同じ並び。1 なら掛かる）
    /// 同じ frustum で続けて呼ぶと、前の結果を返す（1 つの視点で何回もパスを描いても、判定は 1 回）
    /// </summary>
    std::span<const uint8_t> GetVisibility(const Frustum& frustum) const
    {
      if (!visibilityValid_ || !(visibilityFrustum_ == frustum)) {
        visibility_.resize(items_.size());
        for (size_t i = 0; i < items_.size(); ++i) {
          visibility_[i] = frustum.Intersects(items_[i].boundsCenter, items_[i].boundsRadius);
        }
        visibilityFrustum_ = frustum;
        visibilityValid_   = true;
      }
      return visibility_;
    }

  private: //メンバー変数
    std::vector<DrawItem> items_;

    //GetVisibility の結果（描画物を足す・消すと無効になる）
    mutable std::vector<uint8_t> visibility_;
    mutable Frustum              visibilityFrustum_{};
    mutable bool                 visibilityValid_ = false;
  };

} // namespace Tako
