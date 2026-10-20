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
    RenderTexture* objectMask = nullptr;  ///< 描画物の maskId を書いた R8_UINT。nullptr ならマスク付きのエフェクトは飛ばす
    uint32_t       width      = 0;        ///< 入力の解像度
    uint32_t       height     = 0;
  };

  /// <summary>
  /// 名前で登録するポストエフェクトの中身。単パスなら pixelShader と defaultParam だけで作れる
  /// </summary>
  struct PostEffectDefinition {
    std::wstring pixelShader;           ///< 単パス: FullScreen.VS とこの PS で描く。入力は gTexture、パラメータは gParam で受ける
    EffectParam  defaultParam;          ///< 追加したときの値。型でパラメータの種類が決まる
    bool         usesDepth    = false;  ///< 単パス: シーンの深度を gDepthTexture で読む
    bool         hidden       = false;  ///< エフェクトの一覧に出さない

    /// <summary>
    /// 単パス: 実行時に決まる値を param に入れ、gTexture・gParam・gDepthTexture 以外の値を渡す
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
    uint32_t    maskId  = 0;     ///< 0 なら画面全体。それ以外は maskId が一致する描画物の画素だけに掛ける
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
  /// FullScreen.VS と psFile で全画面三角形を描くパイプライン
  /// </summary>
  GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, std::wstring_view psFile);

  /// <summary>
  /// output を描画先・ビューポートにし、FullScreen.VS と psFile のパイプラインを設定する。値を渡してから DrawFullScreen を呼ぶ
  /// </summary>
  CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output);

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
    /// 有効なエフェクトを順に input→output へ掛ける。大きさが違えば最後に拡大コピーする
    /// </summary>
    /// <param name="antiAliasing">最後に FXAA を掛ける</param>
    void Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing = false) const;

    //=========================================
    //Getter
    //=========================================
    PostEffectInstance* Find(uint32_t id);
    std::vector<PostEffectInstance>& GetEffects() { return effects_; }
    const std::vector<PostEffectInstance>& GetEffects() const { return effects_; }

    bool HasEnabledEffects() const;

    /// <summary>
    /// 有効なエフェクトに maskId 付きがあるか（ObjectMask の描画が要るか）
    /// </summary>
    bool UsesObjectMask() const;

  private: //メンバー変数
    std::vector<PostEffectInstance> effects_;
    uint32_t                        nextId_  = 1;
  };

} // namespace Tako
