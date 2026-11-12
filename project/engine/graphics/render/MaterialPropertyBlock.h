#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <json.hpp>

#include "Matrix4x4.h"
#include "ShaderBindingLayout.h"
#include "Vector4.h"

namespace Tako {

  struct MaterialPropertyLayout;

  /// <summary>
  /// 描画物ごとの値。描くときに、マテリアルの定数の写しの同じ名前の値へ重ねる（マテリアル自体は書き換えない。Unity の MaterialPropertyBlock に相当）。
  /// シェーダーに無い名前は無視する
  /// </summary>
  class MaterialPropertyBlock {
  private: //構造体
    struct Value {
      ShaderParamId         id     = 0;
      uint32_t              count  = 0;  ///< data の有効な数（1〜16）
      std::array<float, 16> data{};      ///< Int / UInt のプロパティへは書くときに整数へ変換する（count 以降は 0）
    };

  public: //メンバー関数
    void SetFloat(std::string_view name, float value) { SetValues(name, std::span<const float>(&value, 1)); }
    void SetInt(std::string_view name, int32_t value) { SetFloat(name, static_cast<float>(value)); }
    void SetVector(std::string_view name, const Vector4& value) { SetValues(name, std::span<const float>(&value.x, 4)); }
    void SetMatrix(std::string_view name, const Matrix4x4& value) { SetValues(name, std::span<const float>(&value.m[0][0], 16)); }

    /// <summary>
    /// 値を成分の並びで入れる（プロパティより多い成分は使わず、足りない成分はマテリアルの値のまま）
    /// </summary>
    void SetValues(std::string_view name, std::span<const float> values);

    /// <summary>
    /// JSON の値（名前 → 数値か数値の配列）を入れる。数値でない値は無視する
    /// </summary>
    void SetValues(const nlohmann::json& values);

    /// <summary>
    /// SrvManager のヒープ上のテクスチャを、シェーダーのテクスチャ名に割り当てる
    /// </summary>
    void SetTexture(std::string_view name, uint32_t srvIndex);

    /// <summary>
    /// 名前の値とテクスチャを外す
    /// </summary>
    void Clear(std::string_view name);

    void Clear();

    /// <summary>
    /// upper の値とテクスチャを上に重ねる（同じ名前は upper が勝つ）
    /// </summary>
    void Merge(const MaterialPropertyBlock& upper);

    /// <summary>
    /// layout の並びの定数へ値を書く
    /// </summary>
    void Apply(const MaterialPropertyLayout& layout, std::span<uint8_t> constants) const;

    //=========================================
    //Getter
    //=========================================
    /// <summary>
    /// 名前の値（無ければ nullopt。4 成分に足りない分は 0）
    /// </summary>
    std::optional<Vector4> GetVector(std::string_view name) const;

    /// <summary>
    /// 名前の値かテクスチャがあるか
    /// </summary>
    bool Has(std::string_view name) const;

    std::span<const std::pair<ShaderParamId, uint32_t>> GetTextures() const { return textures_; }
    bool HasValues() const { return !values_.empty(); }
    bool IsEmpty() const { return values_.empty() && textures_.empty(); }

    /// <summary>
    /// 値かテクスチャが変わるたびに増える番号（同じ値を入れ直しても増えない）
    /// </summary>
    uint32_t GetVersion() const { return version_; }

  private: //非公開関数
    void Set(const Value& value);

    void SetTextureById(ShaderParamId id, uint32_t srvIndex);

  private: //メンバー変数
    std::vector<Value>                              values_;
    std::vector<std::pair<ShaderParamId, uint32_t>> textures_;      ///< シェーダーのテクスチャ名と SRV インデックス
    uint32_t                                        version_  = 0;
  };

} // namespace Tako
