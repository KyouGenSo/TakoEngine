#pragma once
#include <cstdint>
#include <d3d12.h>

namespace Tako {

  /// <summary>
  /// 出力色と描画先の合成方法。値はパーティクルプリセットの保存値と一致させている
  /// </summary>
  enum class BlendMode : uint32_t {
    Add           = 0,  ///< src*srcA + dst
    Screen        = 1,  ///< src + dst - src*dst
    Alpha         = 2,  ///< src*srcA + dst*(1-srcA)
    Opaque        = 3,  ///< 合成しない
    Multiply      = 4,  ///< src*dst
    Premultiplied = 5,  ///< src + dst*(1-srcA)（色に α を乗算済みの出力向け）
    Count
  };

  enum class CullMode : uint8_t { None, Front, Back };

  /// <summary>
  /// 頂点バッファのレイアウト。描画するデータの種類ごとに 1 つ
  /// </summary>
  enum class VertexLayout : uint8_t {
    None,              ///< 頂点入力なし（SV_VertexID で生成する全画面描画など）
    Mesh,              ///< POSITION float4 / TEXCOORD float2 / NORMAL float3（VertexData）
    Position,          ///< POSITION float4
    Sprite,            ///< POSITION float4 / TEXCOORD float2
    Line,              ///< POSITION float3 / COLOR float4
    ParticleInstance,  ///< slot1 のインスタンスごとの TEXCOORD1 uint（粒子 index）
  };

  /// <summary>
  /// PSO の固定機能部分のうち、描画物ごとに変えるもの
  /// </summary>
  struct RenderState {
    BlendMode             blend      = BlendMode::Opaque;
    CullMode              cull       = CullMode::Back;
    bool                  depthTest  = true;
    bool                  depthWrite = true;
    D3D12_COMPARISON_FUNC depthFunc  = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    bool operator==(const RenderState&) const = default;
  };

} // namespace Tako
