#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <json.hpp>

#include "ShaderBindingLayout.h"

struct ID3D12ShaderReflection;

namespace Tako {

  class Shader;

  /// <summary>
  /// シェーダーの定数バッファの 1 メンバー（リフレクションで得る）
  /// </summary>
  struct MaterialProperty {
    enum class Type : uint8_t { Float, Int, UInt, Unsupported };

    std::string   name;                      ///< 入れ子の構造体は "a.b"
    ShaderParamId id         = 0;            ///< name の FNV-1a
    Type          type       = Type::Float;
    uint32_t      components = 1;            ///< 1〜4（ベクトルの要素数）。行列は 行数×4
    uint32_t      offset     = 0;            ///< 定数バッファ先頭からのバイト数
    bool          isColor    = false;        ///< 名前に color を含む float3/float4（色の編集 UI にする）
    float         min        = 0.0f;         ///< min == max なら範囲なし
    float         max        = 0.0f;
  };

  /// <summary>
  /// マテリアル（またはポストエフェクト）が持つ値の並び。定数バッファのメンバーとテクスチャを名前で引ける
  /// </summary>
  struct MaterialPropertyLayout {
    uint32_t                      constantSize = 0;  ///< 定数バッファの大きさ（0 なら定数なし）
    std::vector<MaterialProperty> properties;
    std::vector<std::string>      textures;          ///< Texture2D と TextureCube の名前（エンジンが描画時に渡すものは除く）

    /// <summary>
    /// ps の cbufferName の定数バッファとテクスチャから作る
    /// </summary>
    /// <param name="ranges">プロパティ名ごとの編集範囲（.shader.json の Properties）</param>
    static std::shared_ptr<MaterialPropertyLayout> Reflect(ID3D12ShaderReflection* ps, std::string_view cbufferName, const std::map<std::string, std::pair<float, float>>& ranges);

    /// <summary>
    /// ps の gMaterial の並び（編集範囲なし）。PS ごとに持ち、ホットリロードでバージョンが上がったら作り直す
    /// </summary>
    static std::shared_ptr<const MaterialPropertyLayout> ForShader(const Shader& ps);

    /// <summary>
    /// 名前の ID のプロパティ（無ければ nullptr）
    /// </summary>
    const MaterialProperty* Find(ShaderParamId id) const;

    /// <summary>
    /// 値を名前で JSON にする（Unsupported の型は書かない）
    /// </summary>
    nlohmann::json SerializeValues(std::span<const uint8_t> constants) const;

    /// <summary>
    /// JSON の値を名前で読み込む。JSON に無い名前・型の合わない値は今の値のまま残す
    /// </summary>
    void DeserializeValues(const nlohmann::json& values, std::span<uint8_t> constants) const;

    /// <summary>
    /// 値の編集 UI を描く（Debug のみ）
    /// </summary>
    /// <returns>値を変えたら true</returns>
    bool DrawImGui(std::span<uint8_t> constants) const;

    /// <summary>
    /// 1 つの値の編集 UI を描く（Debug のみ）
    /// </summary>
    /// <returns>値を変えたら true</returns>
    bool DrawPropertyImGui(const MaterialProperty& property, std::span<uint8_t> constants) const;

    /// <summary>
    /// 1 つの値を JSON（SerializeValues の 1 項目と同じ形）のまま編集する UI を描く（Debug のみ）
    /// </summary>
    /// <returns>値を変えたら true</returns>
    bool DrawValueImGui(const MaterialProperty& property, nlohmann::json& value) const;
  };

  /// <summary>
  /// レイアウトと値の組。C++ の構造体を持たないポストエフェクトのパラメータに使う
  /// </summary>
  struct PropertyBlock {
    std::shared_ptr<const MaterialPropertyLayout> layout;
    std::vector<uint8_t>                          bytes;
  };

} // namespace Tako
