#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <json.hpp>

#include "MaterialPropertyBlock.h"

namespace Tako {

  class Material;

  /// <summary>
  /// マテリアルの差し替えと値の上書き（全メッシュ用とメッシュ名ごとで共通）
  /// </summary>
  struct MaterialOverrides {
    std::optional<std::string>         material;                             ///< resources/Materials のマテリアル名（空ならモデルの既定のマテリアル）
    nlohmann::json                     values   = nlohmann::json::object();  ///< 上書きする値（シェーダーの定数名 → 値。無い値は引き継ぐ）
    std::map<std::string, std::string> textures;                             ///< 上書きするテクスチャ（シェーダーのテクスチャ名 → ファイル）
  };

  /// <summary>
  /// メッシュ名ごとの設定。Material はこちらを優先し、Values と Textures は全メッシュ用の上に重ねる
  /// </summary>
  struct MeshOverrides : MaterialOverrides {
    std::optional<bool> visible;
  };

  /// <summary>
  /// 1 つのメッシュに当てるマテリアルと値（ObjectOverrides::ResolveMaterials で作る。保存しない）
  /// </summary>
  struct ResolvedMaterial {
    std::shared_ptr<Material> material;    ///< 共有のマテリアル（nullptr ならモデルの既定のマテリアル）
    MaterialPropertyBlock     properties;  ///< Values と Textures（描くときにマテリアルの値へ重ねる）
  };

  /// <summary>
  /// 描画物 1 つの描画設定。値の無い項目は既定を使う（Object3d のコードで Set した値はこれより優先）。
  ///
  /// 保存する形
  ///   { "Visible": true, "MaskId": 1, "SortingOrder": 0, "CastShadow": false, "Transparent": true, "PostEffect": false,
  ///     "Material": "Hologram", "Values": { "color": [1, 0, 0, 1] }, "Textures": { "gTexture": "noise.png" },
  ///     "Meshes": { "Sword": { "Visible": false, "Material": "XRay", "Values": {}, "Textures": {} } } }
  ///   Values と Textures はマテリアルを書き換えず、描くときにそのメッシュを描くマテリアルの同じ名前の値へ重ねる
  /// </summary>
  struct ObjectOverrides : MaterialOverrides {
    std::optional<bool>                                  visible;
    std::optional<uint32_t>                              maskId;
    std::optional<int32_t>                               sortingOrder;
    std::optional<bool>                                  castShadow;
    std::optional<bool>                                  transparent;
    std::optional<bool>                                  postEffect;
    std::map<std::string, MeshOverrides, std::less<>>    meshes;          ///< メッシュ名ごと
    ResolvedMaterial                                     resolved;        ///< 全メッシュ用（保存しない）
    std::map<std::string, ResolvedMaterial, std::less<>> resolvedMeshes;  ///< マテリアルを設定したメッシュ名ごと（保存しない）

    /// <summary>
    /// メッシュに当てるマテリアル（メッシュ名のマテリアル設定が無ければ全メッシュ用）
    /// </summary>
    const ResolvedMaterial& Resolve(std::string_view mesh) const;

    /// <summary>
    /// 保存する形の JSON にする（値の無い項目は書かない）
    /// </summary>
    nlohmann::json Save() const;

    /// <summary>
    /// Save の形から読む（型が違えば nlohmann::json の例外を投げる）
    /// </summary>
    static ObjectOverrides Load(const nlohmann::json& entry);

    /// <summary>
    /// マテリアルを読み、resolved と resolvedMeshes を作り直す
    /// </summary>
    void ResolveMaterials();
  };

} // namespace Tako
