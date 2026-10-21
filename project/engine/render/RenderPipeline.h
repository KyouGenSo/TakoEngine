#pragma once
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "RenderGraph.h"
#include "Vector4.h"

namespace Tako {

  class DX12Basic;
  class Material;

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
  /// オブジェクトパスで描く描画物を、描画キューで絞る
  /// </summary>
  enum class QueueFilter : uint8_t {
    All,
    Opaque,       ///< Transparent（3000）未満
    Transparent,  ///< Transparent（3000）以上
    Count
  };

  // 保存ファイルと編集 UI で使う名前。並びは列挙の値と一致させる
  inline constexpr const char* kRenderEventNames[] = { "BeforeShadow", "AfterOpaque", "BeforeTransparent", "AfterTransparent", "BeforePostProcess", "AfterPostProcess", "BeforeUI", "AfterUI" };
  inline constexpr const char* kQueueFilterNames[] = { "All", "Opaque", "Transparent" };
  static_assert(std::size(kRenderEventNames) == static_cast<size_t>(RenderEvent::Count));
  static_assert(std::size(kQueueFilterNames) == static_cast<size_t>(QueueFilter::Count));

  /// <summary>
  /// データで足す描画パス（Unity URP の Render Objects に相当）。event の位置で、objects の描画物を material で描き直す
  /// </summary>
  struct ObjectPassSettings {
    std::string              name;
    RenderEvent              event   = RenderEvent::AfterTransparent;
    bool                     enabled = true;
    QueueFilter              queue   = QueueFilter::All;
    std::vector<std::string> objects;   ///< Object3d の名前（空なら全ての描画物）
    std::string              material;  ///< resources/Materials のマテリアル名（空なら各描画物のマテリアル）
    bool                     replace = false;  ///< objects を本来の描画（不透明・半透明）から外し、このパスでだけ描く（影とマスクは本来のまま）
  };

  /// <summary>
  /// 1 フレームの描画の手順 1 つ（エディタの一覧用）
  /// </summary>
  struct FrameStep {
    std::string label;
    uint32_t    drawCalls = 0;
    uint8_t     depth     = 0;      ///< 0 は組み込みの手順、1 は直前の位置に差し込んだパス
    bool        isEvent   = false;  ///< パスを差し込める位置
  };

  /// <summary>
  /// 本編 1 フレームの描画（影→不透明→デカール→半透明→パーティクル→線→マスク→ポストエフェクト→UI）を順に行う。
  /// シーン色と最終結果の RT、一時 RT のプール、差し込んだパスを持つ。
  ///
  /// resources/Json/RenderPasses.json（オブジェクトパス。並びは同じ位置での実行順）
  ///   { "Passes": [ { "Name": "PlayerXRay", "Event": "AfterTransparent", "Enabled": true, "Queue": "All", "Objects": ["Player"], "Material": "XRay", "Replace": false } ] }
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
      std::string           name;
      std::function<void()> execute;
    };

    /// <summary>
    /// ObjectPassSettings の名前を解決したもの
    /// </summary>
    struct ObjectPass {
      std::shared_ptr<Material> material;   ///< nullptr なら各描画物のマテリアル
      std::vector<uint32_t>     objectIds;  ///< 名前の FNV-1a（DrawItem::nameId と比べる）
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
    /// event の位置で execute を呼ぶ（同じ位置は追加順、オブジェクトパスより先）。描画先はその位置の描画先のまま渡す
    /// </summary>
    /// <param name="name">フレームの手順の一覧に出す名前</param>
    /// <returns>RemoveRenderPass に渡す id</returns>
    uint32_t AddRenderPass(RenderEvent event, std::string name, std::function<void()> execute);

    void RemoveRenderPass(uint32_t id);

    /// <summary>
    /// resources/Json/RenderPasses.json のオブジェクトパスを読む（無ければ無し）。MaterialLibrary の初期化後に呼ぶ
    /// </summary>
    void LoadObjectPasses();

    bool SaveObjectPasses() const;

    /// <summary>
    /// SaveObjectPasses が書く内容（JSON 文字列）
    /// </summary>
    std::string SerializeObjectPasses() const;

    /// <summary>
    /// SerializeObjectPasses の内容でオブジェクトパスを置き換える
    /// </summary>
    /// <returns>読めなければ false（今のまま）</returns>
    bool ApplyObjectPasses(const std::string& text);

    /// <summary>
    /// オブジェクトパスの設定を書き換えた後に呼び、マテリアルと名前を解決し直す
    /// </summary>
    void RefreshObjectPasses();

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
    std::vector<ObjectPassSettings>& GetObjectPasses() { return objectPassSettings_; }

    /// <summary>
    /// UI まで重ねた最終結果の SRV（ImGui のゲーム画面表示用）
    /// </summary>
    uint32_t GetFinalSrvIndex() const { return final_.srvIndex; }

    /// <summary>
    /// 前のフレームに行った描画の手順（Debug のみ記録する）
    /// </summary>
    std::span<const FrameStep> GetFrameSteps() const { return frameSteps_; }

  private: //非公開関数
    void CreateRenderTargets();

    void ReleaseRenderTargets();

    /// <summary>
    /// event の位置に差し込んだパス（コード → オブジェクトパスの順）を実行する
    /// </summary>
    void RunPasses(RenderEvent event);

    /// <summary>
    /// 手順を記録する（Debug のみ）
    /// </summary>
    /// <param name="firstDraw">手順を始めたときの CommandContext::GetDrawCount</param>
    void RecordStep(std::string_view label, uint32_t firstDraw, uint8_t depth = 0, bool isEvent = false);

  private: //メンバー変数
    DX12Basic*    dx12_          = nullptr;
    RenderGraph   graph_;
    RenderTexture sceneColor_;                                     ///< ポストエフェクト前のシーン（シーン解像度）
    RenderTexture final_;                                          ///< ポストエフェクト後に UI を重ねた結果（ウィンドウ解像度）
    uint32_t      depthSrvIndex_ = 0;                              ///< シーン深度の SRV（ポストエフェクトが読む）
    Vector4       clearColor_    = { 0.17f, 0.17f, 0.17f, 1.0f };

    //差し込んだパス
    std::vector<InjectedPass>       passes_;
    uint32_t                        nextPassId_ = 1;
    std::vector<ObjectPassSettings> objectPassSettings_;
    std::vector<ObjectPass>         objectPasses_;      ///< objectPassSettings_ と同じ並び
    std::vector<uint32_t>           replacedIds_;       ///< Replace のパスで描くので、本来の描画から外す名前の FNV-1a

    std::vector<FrameStep> frameSteps_;
  };

} // namespace Tako
