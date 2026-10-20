#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "RenderGraph.h"
#include "Vector4.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// 本編の描画で追加のパスを差し込める位置（Unity の RenderPassEvent に相当）
  /// </summary>
  enum class RenderEvent : uint8_t {
    BeforeShadow,
    AfterOpaque,        ///< 不透明の直後（デカールの前）
    BeforeTransparent,  ///< デカールの後、半透明の前
    AfterTransparent,   ///< 半透明・パーティクル・線の後
    BeforePostProcess,
    AfterPostProcess,   ///< 描画先は最終結果の RT（UI の前）
    BeforeUI,
    AfterUI,
    Count
  };

  /// <summary>
  /// 本編 1 フレームの描画（影→不透明→デカール→半透明→パーティクル→線→マスク→ポストエフェクト→UI）を順に行う。
  /// シーン色と最終結果の RT、一時 RT のプールを持つ
  /// </summary>
  class RenderPipeline {
  private: // シングルトン設定
    static std::unique_ptr<RenderPipeline> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~RenderPipeline() = default;
    RenderPipeline(RenderPipeline&) = delete;
    RenderPipeline& operator=(RenderPipeline&) = delete;

    friend struct std::default_delete<RenderPipeline>;

  public:
    explicit RenderPipeline(Token) {}

  private: //構造体
    struct InjectedPass {
      uint32_t              id;
      RenderEvent           event;
      std::function<void()> execute;
    };

  public: //メンバー関数
    static RenderPipeline* GetInstance();

    void Initialize(DX12Basic* dx12);

    void Finalize();

    /// <summary>
    /// 本編を描く。シーンと UI の描画物は Renderer の各リストに登録済みであること
    /// </summary>
    void Render();

    /// <summary>
    /// バックバッファを描画先にする
    /// </summary>
    /// <param name="copyFinalResult">最終結果をバックバッファへ写す（ImGui のウィンドウでゲーム画面を見せるときは false）</param>
    void Present(bool copyFinalResult);

    /// <summary>
    /// シーン色・最終結果の RT をウィンドウとシーンの解像度で作り直す
    /// </summary>
    void OnResize();

    /// <summary>
    /// event の位置で execute を呼ぶ（同じ位置は追加順）。描画先はその位置の描画先のまま渡す
    /// </summary>
    /// <returns>RemoveRenderPass に渡す id</returns>
    uint32_t AddRenderPass(RenderEvent event, std::function<void()> execute);

    void RemoveRenderPass(uint32_t id);

    //=========================================
    //Setter
    //=========================================
    /// <summary>
    /// シーン色のクリア色を設定し、RT の最適化クリア値を揃えるため作り直しを要求する
    /// </summary>
    void SetClearColor(const Vector4& color);

    //=========================================
    //Getter
    //=========================================
    const Vector4& GetClearColor() const { return clearColor_; }
    RenderGraph& GetGraph() { return graph_; }
    RenderTexture& GetSceneColor() { return sceneColor_; }

    /// <summary>
    /// UI まで重ねた最終結果の SRV（ImGui のゲーム画面表示用）
    /// </summary>
    uint32_t GetFinalSrvIndex() const { return final_.srvIndex; }

  private: //非公開関数
    void CreateRenderTargets();

    void ReleaseRenderTargets();

    void RunPasses(RenderEvent event);

  private: //メンバー変数
    DX12Basic*    dx12_          = nullptr;
    RenderGraph   graph_;
    RenderTexture sceneColor_;                                     ///< ポストエフェクト前のシーン（シーン解像度）
    RenderTexture final_;                                          ///< ポストエフェクト後に UI を重ねた結果（ウィンドウ解像度）
    uint32_t      depthSrvIndex_ = 0;                              ///< シーン深度の SRV（ポストエフェクトが読む）
    Vector4       clearColor_    = { 0.17f, 0.17f, 0.17f, 1.0f };

    //差し込んだパス
    std::vector<InjectedPass> passes_;
    uint32_t                  nextPassId_ = 1;
  };

} // namespace Tako
