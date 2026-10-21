#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>
#include <wrl.h>

#include "CommandContext.h"
#include "ShaderAsset.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// シェーダーと、その描画設定・定数・テクスチャの組。描画物はマテリアルを差し替えるだけで別のシェーダーで描ける
  /// </summary>
  class Material {
  public: //メンバー関数
    /// <summary>
    /// シェーダーの既定の描画設定と描画キューで作る
    /// </summary>
    /// <param name="dx12">定数バッファの生成に使う</param>
    /// <param name="shader">使うシェーダー</param>
    Material(DX12Basic* dx12, const ShaderAsset* shader);

    /// <summary>
    /// 定数を別の定数バッファへ写した複製を作る
    /// </summary>
    std::unique_ptr<Material> Clone() const;

    /// <summary>
    /// 定数（シェーダー側の gMaterial）を型として読み書きする。初めて呼んだときに sizeof(T) で確保する
    /// </summary>
    template<class T>
    T& Constants()
    {
      if (!constants_) {
        AllocateConstants(sizeof(T));
      }
      assert(constantSize_ == sizeof(T) && "Material: constant type size mismatch");
      return *reinterpret_cast<T*>(constants_);
    }

    template<class T>
    const T& Constants() const
    {
      assert(constants_ && constantSize_ == sizeof(T) && "Material: constant type size mismatch");
      return *reinterpret_cast<const T*>(constants_);
    }

    /// <summary>
    /// 定数をバイト列で扱う（リフレクションのレイアウトで値を読み書きするマテリアル用）。
    /// size が今と違えば 0 で埋めて確保し直す（値は呼び出し側で入れ直す）。Constants と同じマテリアルで混ぜない
    /// </summary>
    std::span<uint8_t> GetConstantBytes(size_t size);

    /// <summary>
    /// 定数とテクスチャを現在のパイプラインへ渡す
    /// </summary>
    void Bind(CommandContext::ShaderBinder& binder) const;

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

    void SetBlendMode(BlendMode blend) { renderState_.blend = blend; }
    void SetCullMode(CullMode cull) { renderState_.cull = cull; }
    void SetDepthTest(bool depthTest) { renderState_.depthTest = depthTest; }
    void SetDepthWrite(bool depthWrite) { renderState_.depthWrite = depthWrite; }
    void SetDepthFunc(D3D12_COMPARISON_FUNC depthFunc) { renderState_.depthFunc = depthFunc; }
    void SetRenderQueue(int32_t queue) { renderQueue_ = queue; }

    /// <summary>
    /// シェーダーを替え、描画設定と描画キューをそのシェーダーの既定に戻す（定数とテクスチャは残す）
    /// </summary>
    void SetShader(const ShaderAsset* shader);

    //=========================================
    //Getter
    //=========================================
    const ShaderAsset* GetShader() const { return shader_; }
    std::span<const uint8_t> GetConstantBytes() const { return { constants_, constantSize_ }; }
    const RenderState& GetRenderState() const { return renderState_; }
    int32_t GetRenderQueue() const { return renderQueue_; }

    /// <summary>
    /// 名前に割り当てたテクスチャの SRV インデックス（未割り当てなら 0）
    /// </summary>
    uint32_t GetTexture(ShaderParamId id) const;

  private: //非公開関数
    void AllocateConstants(size_t size);

  private: //メンバー変数
    DX12Basic*         dx12_   = nullptr;
    const ShaderAsset* shader_ = nullptr;

    //描画設定
    RenderState renderState_{};                                ///< Forward パスで使う（シェーダーの既定から上書きできる）
    int32_t     renderQueue_   = ShaderAsset::kQueueGeometry;

    //定数
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    uint8_t*                               constants_      = nullptr;  ///< 常駐マップ先
    size_t                                 constantSize_   = 0;

    std::vector<std::pair<ShaderParamId, uint32_t>> textures_;  ///< シェーダーの名前と SRV インデックス
  };

} // namespace Tako
