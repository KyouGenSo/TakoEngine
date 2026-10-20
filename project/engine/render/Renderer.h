#pragma once
#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "DrawList.h"
#include "Light.h"
#include "Matrix4x4.h"
#include "ShaderAsset.h"
#include "Vector2.h"

namespace Tako {

  class Camera;
  class DX12Basic;

  /// <summary>
  /// 描画物の登録を受け付け、シェーダーのパスごとに並べ替えて描く。視点・ライト・シャドウなど描画物に共通の値も渡す
  /// </summary>
  class Renderer {
  private: // シングルトン設定
    static std::unique_ptr<Renderer> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~Renderer() = default;
    Renderer(Renderer&) = delete;
    Renderer& operator=(Renderer&) = delete;

    friend struct std::default_delete<Renderer>;

  public:
    explicit Renderer(Token) {}

  public: //構造体
    /// <summary>
    /// 視点ごとの定数（View.hlsli と一致させること）
    /// </summary>
    struct ViewConstants {
      Matrix4x4 viewProjection;
      Vector3   worldPosition;
      float     padding;
    };

    /// <summary>
    /// 描画に使う視点。定数はそのフレームのアップロード領域にある
    /// </summary>
    struct RenderView {
      D3D12_GPU_VIRTUAL_ADDRESS constants = 0;  ///< gView
      Vector3                   position{};      ///< 半透明の並べ替えに使うカメラ位置
    };

  private: //構造体
    struct SortEntry {
      int32_t  queue;
      int32_t  sortingOrder;
      float    depthKey;  ///< 小さいほど先（奥から手前の並べ替えでは距離の符号を反転して入れる）
      uint32_t index;
    };

  public: //メンバー関数
    static Renderer* GetInstance();

    /// <summary>
    /// 組み込みのシェーダーとライトを用意する。モデルの読み込みより前に呼ぶ
    /// </summary>
    void Initialize(DX12Basic* dx12);

    void Finalize();

    /// <summary>
    /// 本編カメラの視点とライトを更新する
    /// </summary>
    void Update();

    /// <summary>
    /// 前フレームの登録を捨てる
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// 以降の Submit の登録先を切り替える
    /// </summary>
    void BeginSubmit(DrawList& list) { current_ = &list; }

    void EndSubmit() { current_ = nullptr; }

    /// <summary>
    /// 描画物を現在の登録先へ積む
    /// </summary>
    void Submit(const DrawItem& item);

    /// <summary>
    /// 影を落とす描画物をシャドウマップへ描く。終わると呼び出し前の描画先へ戻る
    /// </summary>
    void RenderShadows(const DrawList& list);

    /// <summary>
    /// 描画キューが [minQueue, maxQueue) の描画物を、描画キュー・sortingOrder・奥行きの順に並べて現在の描画先へ描く
    /// </summary>
    void RenderList(const DrawList& list, const RenderView& view, int32_t minQueue = INT32_MIN, int32_t maxQueue = INT32_MAX);

    /// <summary>
    /// maskId を持つ描画物の maskId を、現在の描画先（R8_UINT の ObjectMask）へ書く
    /// </summary>
    void RenderMask(const DrawList& list, const RenderView& view);

    /// <summary>
    /// submit 内で登録した描画物だけを、その場で指定の視点で描く（エディタのプレビュー用）
    /// </summary>
    void RenderImmediate(const RenderView& view, const std::function<void()>& submit);

    /// <summary>
    /// カメラの視点定数をこのフレームのアップロード領域に書く
    /// </summary>
    RenderView MakeView(const Camera& camera);

    /// <summary>
    /// シェーダーを名前で登録する（同名があればそれを返す）
    /// </summary>
    ShaderAsset& RegisterShader(const std::string& name);

    /// <summary>
    /// ウィンドウサイズに合わせてスクリーン空間の射影を作り直す
    /// </summary>
    void OnResize(const Vector2& size);

    //=========================================
    //Setter
    //=========================================
    void SetCamera(Camera* camera) { camera_ = camera; }

    //=========================================
    //Getter
    //=========================================
    DX12Basic* GetDX12Basic() const { return dx12_; }
    Camera** GetCamera() { return &camera_; }
    Light* GetLight() const { return light_.get(); }
    const RenderView& GetMainView() const { return mainView_; }
    DrawList& GetSceneList() { return sceneList_; }
    DrawList& GetOverlayList() { return overlayList_; }
    const Matrix4x4& GetScreenViewMatrix() const { return screenView_; }
    const Matrix4x4& GetScreenProjectionMatrix() const { return screenProjection_; }

    /// <summary>
    /// 名前で登録したシェーダー（無ければ nullptr）
    /// </summary>
    const ShaderAsset* FindShader(std::string_view name) const;

    /// <summary>
    /// 登録したシェーダーの名前（名前順）
    /// </summary>
    std::vector<std::string> GetShaderNames() const;

  private: //非公開関数
    void RegisterBuiltinShaders();

    /// <summary>
    /// 視点・ライト・シャドウを現在のパイプラインへ渡す
    /// </summary>
    void BindGlobals(const RenderView& view);

    /// <summary>
    /// ジオメトリと描画物ごとの値を渡して描く
    /// </summary>
    void DrawItemGeometry(const DrawItem& item, bool bindMaterial);

  private: //メンバー変数
    DX12Basic*             dx12_   = nullptr;
    Camera*                camera_ = nullptr;  ///< 本編カメラ
    std::unique_ptr<Light> light_;
    RenderView             mainView_{};       ///< Update で本編カメラから作る

    //スクリーン空間（スプライト）
    Matrix4x4 screenView_{};
    Matrix4x4 screenProjection_{};

    //登録
    DrawList               sceneList_;              ///< ポストエフェクトを掛ける描画物
    DrawList               overlayList_;            ///< ポストエフェクトの後に重ねる描画物
    DrawList*              current_     = nullptr;  ///< Submit の登録先
    std::vector<SortEntry> sortEntries_;            ///< RenderList の作業領域

    std::unordered_map<std::string, std::unique_ptr<ShaderAsset>> shaders_;
  };

} // namespace Tako
