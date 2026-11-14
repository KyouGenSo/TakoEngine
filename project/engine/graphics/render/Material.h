#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "CommandContext.h"
#include "ShaderAsset.h"

namespace Tako {

  /// <summary>
  /// シェーダーと、その描画設定・定数・テクスチャの組。描画物はマテリアルを差し替えるだけで別のシェーダーで描ける
  /// 定数は CPU のメモリに持ち、描くたびに Renderer が 1 フレーム用のバッファへ写して渡す
  /// </summary>
  class TAKO_API Material {
  public: //メンバー関数
    /// <summary>
    /// 描画設定と描画キューはシェーダーの値を使う（Set* で上書きできる）
    /// </summary>
    /// <param name="shader">使うシェーダー</param>
    explicit Material(const ShaderAsset* shader);

    /// <summary>
    /// 定数とテクスチャまで写した複製を作る
    /// </summary>
    std::unique_ptr<Material> Clone() const;

    /// <summary>
    /// source のシェーダー・描画設定・定数・テクスチャを写す（定数の大きさが同じなら今の領域へ書く）
    /// </summary>
    void CopyFrom(const Material& source);

    /// <summary>
    /// 定数（シェーダー側の gMaterial）を型として読み書きする。初めて呼んだときに sizeof(T) で確保する
    /// </summary>
    template<class T>
    T& Constants()
    {
      if (constants_.empty()) {
        constants_.resize(sizeof(T));
      }
      assert(constants_.size() == sizeof(T) && "Material: constant type size mismatch");
      return *reinterpret_cast<T*>(constants_.data());
    }

    template<class T>
    const T& Constants() const
    {
      assert(constants_.size() == sizeof(T) && "Material: constant type size mismatch");
      return *reinterpret_cast<const T*>(constants_.data());
    }

    /// <summary>
    /// 定数をバイト列で扱う（リフレクションのレイアウトで値を読み書きするマテリアル用）。
    /// size が今と違えば 0 で埋めて確保し直す（値は呼び出し側で入れ直す）。Constants と同じマテリアルで混ぜない
    /// </summary>
    std::span<uint8_t> GetConstantBytes(size_t size);

    /// <summary>
    /// パスのパイプライン。Forward はこのマテリアルで上書きした描画設定を使う
    /// </summary>
    /// <param name="pass">描画パス</param>
    /// <param name="instanced">インスタンシング版か</param>
    /// <param name="pipeline">結果の書き込み先</param>
    /// <returns>シェーダーがそのパスを持たなければ false</returns>
    bool GetPipeline(ShaderPass pass, bool instanced, GraphicsPipelineDesc& pipeline) const;

    //=========================================
    //Setter
    //=========================================
    /// <summary>
    /// SrvManager のヒープ上のテクスチャをシェーダーの名前に割り当てる（0 で解除）
    /// </summary>
    void SetTexture(ShaderParamId id, uint32_t srvIndex);

    void SetBlendMode(BlendMode blend) { stateOverride_.blend = blend; }
    void SetCullMode(CullMode cull) { stateOverride_.cull = cull; }
    void SetDepthTest(bool depthTest) { stateOverride_.depthTest = depthTest; }
    void SetDepthWrite(bool depthWrite) { stateOverride_.depthWrite = depthWrite; }
    void SetDepthFunc(D3D12_COMPARISON_FUNC depthFunc) { stateOverride_.depthFunc = depthFunc; }
    void SetRenderQueue(int32_t queue) { queueOverride_ = queue; }

    /// <summary>
    /// 描画設定の上書きをまとめて置き換える（値の無い項目はシェーダーの値を使う）
    /// </summary>
    void SetStateOverride(const RenderStateOverride& stateOverride) { stateOverride_ = stateOverride; }

    /// <summary>
    /// 描画キューの上書きを置き換える（nullopt ならシェーダーの描画キューを使う）
    /// </summary>
    void SetQueueOverride(std::optional<int32_t> queue) { queueOverride_ = queue; }

    /// <summary>
    /// シェーダーを替える（描画設定の上書き・定数・テクスチャは残す）
    /// </summary>
    void SetShader(const ShaderAsset* shader);

    //=========================================
    //Getter
    //=========================================
    const ShaderAsset* GetShader() const { return shader_; }
    std::span<const uint8_t> GetConstantBytes() const { return constants_; }
    std::span<const std::pair<ShaderParamId, uint32_t>> GetTextures() const { return textures_; }
    const RenderStateOverride& GetStateOverride() const { return stateOverride_; }
    std::optional<int32_t> GetQueueOverride() const { return queueOverride_; }

    /// <summary>
    /// Forward パスで使う描画設定（シェーダーの値に上書きを重ねたもの）
    /// </summary>
    RenderState GetRenderState() const;

    /// <summary>
    /// 描画キュー（上書きが無ければシェーダーの値）
    /// </summary>
    int32_t GetRenderQueue() const { return queueOverride_.value_or(shader_->GetDefaultQueue()); }

    /// <summary>
    /// 名前に割り当てたテクスチャの SRV インデックス（未割り当てなら 0）
    /// </summary>
    uint32_t GetTexture(ShaderParamId id) const;

  private: //メンバー変数
    const ShaderAsset* shader_ = nullptr;

    //描画設定（シェーダーの Forward パスの値に重ねる）
    RenderStateOverride    stateOverride_;
    std::optional<int32_t> queueOverride_;  ///< 無ければシェーダーの描画キュー

    std::vector<uint8_t>                            constants_;  ///< gMaterial の中身（色は sRGB のまま）
    std::vector<std::pair<ShaderParamId, uint32_t>> textures_;   ///< シェーダーの名前と SRV インデックス
  };

} // namespace Tako
