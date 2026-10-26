#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CommandContext.h"
#include "PostEffectStruct.h"
#include "RenderGraph.h"

namespace Tako {

  class Camera;
  class DX12Basic;
  class ShaderLibrary;

  /// <summary>
  /// 1 回のポストエフェクト実行で全エフェクトが共有する値
  /// </summary>
  struct PostEffectFrame {
    DX12Basic*     dx12       = nullptr;
    RenderGraph*   graph      = nullptr;
    const Camera*  camera     = nullptr;
    GpuResource*   depth      = nullptr;  ///< シーンの深度（入力と同じ解像度）
    uint32_t       depthSrv   = 0;
    uint32_t       width      = 0;        ///< 入力の解像度
    uint32_t       height     = 0;

    D3D12_GPU_VIRTUAL_ADDRESS view = 0;  ///< gView（Renderer::MakeView。0 なら渡さない）
  };

  /// <summary>
  /// 名前で登録するポストエフェクトの中身。単パスなら pixelShader と defaultParam だけで作れる。
  /// 色は α を乗算済みで受け取り、乗算済みのまま返す（足す色・置き換える色に α を掛け、(0,0,0,0) は (0,0,0,0) のままにする）。
  /// 画面全体では α=1、マスク付きでは描画物だけを描いた画像（外は (0,0,0,0)）が入る
  /// </summary>
  struct PostEffectDefinition {
    std::wstring pixelShader;           ///< 単パス: FullScreen.VS とこの PS で描く。入力は gTexture、パラメータは gParam、視点・時間・画面サイズは gView で受ける
    EffectParam  defaultParam;          ///< 追加したときの値。型でパラメータの種類が決まる
    bool         usesDepth    = false;  ///< 単パス: シーンの深度を gDepthTexture で読む
    bool         hidden       = false;  ///< エフェクトの一覧に出さない

    /// <summary>
    /// 単パス: 実行時に決まる値を param に入れ、gTexture・gParam・gDepthTexture・gView 以外の値を渡す
    /// </summary>
    std::function<void(const PostEffectFrame&, EffectParam& param, CommandContext::ShaderBinder&)> setup;

    /// <summary>
    /// 複数パス: input を読んで output へ書く（pixelShader と setup は使わない）
    /// </summary>
    std::function<void(const PostEffectFrame&, const EffectParam& param, RenderTexture& input, RenderTexture& output)> execute;
  };

  /// <summary>
  /// スタックに並べたエフェクト 1 つ。同じ定義を何個並べてもよい
  /// </summary>
  struct PostEffectInstance {
    uint32_t    id      = 0;     ///< スタック内で一意（0 は無効）
    std::string name;            ///< 定義の名前
    EffectParam param;
    uint32_t    maskId  = 0;     ///< 0 なら画面全体。それ以外は maskId が一致する描画物だけを描いた画像に掛けてシーンへ重ねる（描画物の外へも広がる）
    bool        enabled = true;
  };

  /// <summary>
  /// 定義を名前で登録する（同名は上書き）
  /// </summary>
  void RegisterPostEffect(const std::string& name, PostEffectDefinition definition);

  /// <summary>
  /// 名前の定義（無ければ nullptr）
  /// </summary>
  const PostEffectDefinition* FindPostEffect(std::string_view name);

  /// <summary>
  /// 登録順の定義の名前
  /// </summary>
  std::span<const std::string> GetPostEffectNames();

  /// <summary>
  /// param が PropertyBlock で、名前の定義の並び（PS のホットリロードで作り直したもの）と違えば、値を名前で移して定義の並びにする
  /// </summary>
  /// <returns>移したら true</returns>
  bool RefreshPropertyLayout(std::string_view name, EffectParam& param);

  /// <summary>
  /// FullScreen.VS と psFile で全画面三角形を描くパイプライン
  /// </summary>
  GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, std::wstring_view psFile);

  /// <summary>
  /// output を描画先・ビューポートにし、FullScreen.VS と psFile のパイプラインを設定する。値を渡してから DrawFullScreen を呼ぶ
  /// </summary>
  /// <param name="blend">output への合成方法</param>
  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output, BlendMode blend = BlendMode::Opaque);

  /// <summary>
  /// 全画面三角形を描く
  /// </summary>
  void DrawFullScreen(const PostEffectFrame& frame);

  /// <summary>
  /// ポストエフェクトを並べた順に掛ける（Unity の Volume の Override 一覧に相当）
  /// </summary>
  class PostProcessStack {
  public: //メンバー関数
    /// <summary>
    /// 定義の既定値でエフェクトを末尾に追加する
    /// </summary>
    /// <returns>インスタンスの id（未登録の名前なら 0）</returns>
    uint32_t Add(std::string_view name, uint32_t maskId = 0);

    /// <summary>
    /// param でエフェクトを末尾に追加する
    /// </summary>
    /// <returns>インスタンスの id（未登録の名前なら 0）</returns>
    uint32_t Add(std::string_view name, const EffectParam& param, uint32_t maskId = 0);

    void Remove(uint32_t id);

    void Clear() { effects_.clear(); }

    /// <summary>
    /// 全エフェクトに RefreshPropertyLayout を掛ける
    /// </summary>
    void RefreshLayouts();

    /// <summary>
    /// maskId が 0 の有効なエフェクトを順に input→output へ掛ける。大きさが違えば最後に拡大コピーする
    /// </summary>
    /// <param name="antiAliasing">最後に FXAA を掛ける</param>
    void Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing = false) const;

    /// <summary>
    /// maskId の有効なエフェクトを layer へ順に掛け、target の上へ乗算済み α で重ねる
    /// </summary>
    /// <param name="layer">maskId の描画物だけを (0,0,0,0) の上に描いた画像（target と同じ大きさ）</param>
    void ExecuteMasked(const PostEffectFrame& frame, uint32_t maskId, RenderTexture& layer, RenderTexture& target) const;

    //=========================================
    //Getter
    //=========================================
    PostEffectInstance* Find(uint32_t id);
    std::vector<PostEffectInstance>& GetEffects() { return effects_; }
    const std::vector<PostEffectInstance>& GetEffects() const { return effects_; }

    bool HasEnabledEffects() const;

    /// <summary>
    /// 有効なエフェクトが使う 0 以外の maskId（小さい順・重複なし）。この描画物はシーンとは別に描く
    /// </summary>
    std::vector<uint32_t> GetMaskIds() const;

  private: //メンバー変数
    std::vector<PostEffectInstance> effects_;
    uint32_t                        nextId_  = 1;
  };

} // namespace Tako
