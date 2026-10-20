#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "PipelineCache.h"

namespace Tako {

  /// <summary>
  /// 描画パスの種類。描画物はパスごとに、シェーダーが持つパイプラインで描かれる
  /// </summary>
  enum class ShaderPass : uint8_t {
    Forward,       ///< 本編の色を描く
    ShadowCaster,  ///< シャドウマップへ深度だけを描く
    Count
  };

  /// <summary>
  /// 名前付きのシェーダー一式。パスごとに通常版とインスタンシング版のパイプラインを持つ（Unity の Shader と Pass に相当）
  /// </summary>
  class ShaderAsset {
  public: //定数
    // Unity の RenderQueue と同じ目安。小さいほど先に描く
    static constexpr int32_t kQueueBackground  = 1000;
    static constexpr int32_t kQueueGeometry    = 2000;
    static constexpr int32_t kQueueTransparent = 3000;  ///< これ以上は奥から手前へ並べて描く
    static constexpr int32_t kQueueOverlay     = 4000;

  public: //メンバー関数
    explicit ShaderAsset(std::string name) : name_(std::move(name)) {}

    /// <summary>
    /// パスのパイプラインを登録する
    /// </summary>
    /// <param name="pass">登録先のパス</param>
    /// <param name="pipeline">パイプライン（描画先のフォーマットは描画時に決まる）</param>
    /// <param name="instanced">インスタンシング描画（gInstanceData を読む）版か</param>
    void SetPass(ShaderPass pass, const GraphicsPipelineDesc& pipeline, bool instanced = false) { passes_[Index(pass, instanced)] = pipeline; }

    //=========================================
    //Setter
    //=========================================
    void SetDefaultQueue(int32_t queue) { defaultQueue_ = queue; }

    //=========================================
    //Getter
    //=========================================
    const std::string& GetName() const { return name_; }
    int32_t GetDefaultQueue() const { return defaultQueue_; }

    /// <summary>
    /// パスのパイプライン。登録されていなければ nullptr
    /// </summary>
    const GraphicsPipelineDesc* GetPass(ShaderPass pass, bool instanced = false) const
    {
      const GraphicsPipelineDesc& pipeline = passes_[Index(pass, instanced)];
      return pipeline.vs ? &pipeline : nullptr;
    }

  private: //非公開関数
    static size_t Index(ShaderPass pass, bool instanced) { return static_cast<size_t>(pass) * 2 + (instanced ? 1 : 0); }

  private: //メンバー変数
    std::string                                                                  name_;
    std::array<GraphicsPipelineDesc, static_cast<size_t>(ShaderPass::Count) * 2> passes_{};
    int32_t                                                                      defaultQueue_ = kQueueGeometry;  ///< マテリアルを作ったときの描画キュー
  };

} // namespace Tako
