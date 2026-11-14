#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "CommandContext.h"
#include "MaterialPropertyBlock.h"
#include "PostEffectStruct.h"
#include "RenderEvent.h"
#include "RenderGraph.h"

namespace Tako {

  class Camera;
  class DX12Basic;
  class Shader;
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

    bool            linear  = true;  ///< 入力がシーンの色（リニアの HDR）か。false なら表示用の色（sRGB の値。AfterPostProcess 以降）で、gParam の色をリニアに直さない
    ToneMapSettings toneMap;         ///< PostProcessStack::Execute の最後に掛ける
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
    /// 掛ける位置。無ければ半透明の後のポストエフェクトの段。ポストエフェクトの段より前はシーン、後は最終結果（AfterUI は UI まで）に掛ける。
    /// maskId のあるインスタンスはマスクの段で掛けるので使わない
    /// </summary>
    std::optional<RenderEvent> event;

    /// <summary>
    /// 一時エフェクト（PostEffectManager::ApplyTemporaryEffect）で時間とともに 0 へ弱める値の ID（PropertyBlock のパラメータ用）
    /// </summary>
    std::vector<ShaderParamId> fadeValues;

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
  /// .shader.json の "Format" に書ける名前。ポストエフェクトの途中のパスが書く一時 RT のフォーマットになる。
  /// DX12 が描画先と線形フィルタを必ずサポートする、整数でないフォーマット（機能レベル 11.0 の必須）
  /// </summary>
  inline constexpr std::pair<const char*, DXGI_FORMAT> kPostEffectFormats[] = {
    { "R8_UNORM",           DXGI_FORMAT_R8_UNORM },
    { "R8G8_UNORM",         DXGI_FORMAT_R8G8_UNORM },
    { "R8G8B8A8_UNORM",     DXGI_FORMAT_R8G8B8A8_UNORM },
    { "R10G10B10A2_UNORM",  DXGI_FORMAT_R10G10B10A2_UNORM },
    { "R11G11B10_FLOAT",    DXGI_FORMAT_R11G11B10_FLOAT },
    { "R16_FLOAT",          DXGI_FORMAT_R16_FLOAT },
    { "R16G16_FLOAT",       DXGI_FORMAT_R16G16_FLOAT },
    { "R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT },
    { "R32_FLOAT",          DXGI_FORMAT_R32_FLOAT },
    { "R32G32_FLOAT",       DXGI_FORMAT_R32G32_FLOAT },
    { "R32G32B32A32_FLOAT", DXGI_FORMAT_R32G32B32A32_FLOAT },
  };

  /// <summary>
  /// .shader.json のポストエフェクトに使う resources/Materials/{name}.mat.json（UE の Post Process Material の Material Instance に相当）。
  /// MaterialLibrary が持ち、編集するとこれを指す全インスタンスに次のフレームから反映される
  /// </summary>
  struct PostEffectMaterial {
    std::string           name;
    std::string           shader;  ///< 定義の名前
    MaterialPropertyBlock values;  ///< Values と Textures（名前で持つので、PS の値の並びが変わっても移さなくてよい）
  };

  /// <summary>
  /// スタックに並べたエフェクト 1 つ。同じ定義を何個並べてもよい
  /// </summary>
  struct PostEffectInstance {
    uint32_t    id      = 0;     ///< スタック内で一意（0 は無効）
    std::string name;            ///< 定義の名前
    EffectParam param;           ///< 描くときの値。定義が PropertyBlock なら PostProcessStack::ResolveValues が毎フレーム作り直す
    uint32_t    maskId  = 0;     ///< 0 なら画面全体。それ以外は maskId が一致する描画物だけを描いた画像に掛けてシーンへ重ねる（描画物の外へも広がる）
    bool        enabled = true;

    //.shader.json のエフェクトの値（定義の既定値 → material → overrides の順に重ねる）
    std::shared_ptr<const PostEffectMaterial> material;   ///< 無ければ定義の既定値
    MaterialPropertyBlock                     overrides;  ///< このインスタンスだけの値（Unity の Volume の Override に相当）
  };

  /// <summary>
  /// 定義を名前で登録する（同名は上書き）
  /// </summary>
  TAKO_API void RegisterPostEffect(const std::string& name, PostEffectDefinition definition);

  /// <summary>
  /// 名前の定義を外す（無ければ何もしない）。スタックに残ったインスタンスは描かれなくなる
  /// </summary>
  TAKO_API void UnregisterPostEffect(std::string_view name);

  /// <summary>
  /// 名前の定義（無ければ nullptr）
  /// </summary>
  TAKO_API const PostEffectDefinition* FindPostEffect(std::string_view name);

  /// <summary>
  /// 登録順の定義の名前
  /// </summary>
  TAKO_API std::span<const std::string> GetPostEffectNames();

  /// <summary>
  /// 定義が PropertyBlock なら、effect.param を定義の既定値に material と overrides を重ねて作り直す（型付きの定義なら何もしない）。
  /// material のシェーダーを替えていれば name も追従させる
  /// </summary>
  TAKO_API void ResolvePostEffectValues(PostEffectInstance& effect);

  /// <summary>
  /// FullScreen.VS と psFile で全画面三角形を描くパイプライン
  /// </summary>
  TAKO_API GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, std::wstring_view psFile);

  /// <summary>
  /// FullScreen.VS と ps で全画面三角形を描くパイプライン
  /// </summary>
  TAKO_API GraphicsPipelineDesc MakeFullScreenPipeline(ShaderLibrary& library, const Shader& ps);

  /// <summary>
  /// output を描画先・ビューポートにし、FullScreen.VS と psFile のパイプラインを設定する。値を渡してから DrawFullScreen を呼ぶ
  /// </summary>
  /// <param name="blend">output への合成方法</param>
  TAKO_API CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, std::wstring_view psFile, RenderTexture& output, BlendMode blend = BlendMode::Opaque);

  /// <summary>
  /// BeginFullScreen と同じだが、エントリポイントや define を指定してコンパイルした ps を使う
  /// </summary>
  TAKO_API CommandContext::ShaderBinder BeginFullScreen(const PostEffectFrame& frame, const Shader& ps, RenderTexture& output, BlendMode blend = BlendMode::Opaque);

  /// <summary>
  /// 全画面三角形を描く
  /// </summary>
  TAKO_API void DrawFullScreen(const PostEffectFrame& frame);

  /// <summary>
  /// source を一時 RT へ写して返す（返した RT はシェーダーリソース、source は描画先の状態になる）。使い終えたら frame.graph へ Release する
  /// </summary>
  TAKO_API RenderTexture& CopyToTemporary(const PostEffectFrame& frame, RenderTexture& source);

  /// <summary>
  /// source（シーンの色。リニアの HDR）に frame.toneMap の露出とトーンマップを掛け、sRGB の値にして output（表示用）へ書く。
  /// 大きさが違えば線形補間で拡大・縮小する
  /// </summary>
  TAKO_API void ToneMapToDisplay(const PostEffectFrame& frame, RenderTexture& source, RenderTexture& output);

  /// <summary>
  /// ポストエフェクトを並べた順に掛ける（Unity の Volume の Override 一覧に相当）
  /// </summary>
  class TAKO_API PostProcessStack {
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
    /// 全エフェクトに ResolvePostEffectValues を掛ける（毎フレーム、描く前に呼ぶ）
    /// </summary>
    void ResolveValues();

    /// <summary>
    /// maskId が 0 で定義に event の無い有効なエフェクトを順に input に掛け、最後に ToneMapToDisplay で output（表示用）へ書く
    /// </summary>
    /// <param name="antiAliasing">トーンマップの前に FXAA を掛ける</param>
    /// <param name="afterEffects">エフェクトを掛け終えた input と同じ解像度の結果を受け取り、トーンマップの前に描き足す（空なら呼ばない）</param>
    void Execute(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, bool antiAliasing = false,
                 const std::function<void(RenderTexture&)>& afterEffects = {}) const;

    /// <summary>
    /// 定義の event が一致する maskId 0 の有効なエフェクトを target へ順に掛ける（一時 RT へ書いてから target へ写す）
    /// </summary>
    /// <returns>掛けたら true（target は描画先、深度はシェーダーリソースの状態になり得る）</returns>
    bool ExecuteAt(const PostEffectFrame& frame, RenderEvent event, RenderTexture& target) const;

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
