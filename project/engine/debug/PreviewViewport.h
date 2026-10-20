#pragma once

#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "Vector4.h"
#include "GpuResource.h"
#include "RenderGraph.h"

namespace Tako {

  class Camera;
  class DX12Basic;
  class PostProcessStack;

  /// <summary>
  /// エディタプレビュー用オフスクリーン描画先一式（カラーRT/深度/RTV/DSV/SRV）と ImGui 表示ヘルパ
  /// </summary>
  class PreviewViewport {
  public: //メンバー関数
    PreviewViewport() = default;
    ~PreviewViewport();

    /// <summary>
    /// RT/深度/各ディスクリプタを生成する（生成済みなら no-op）
    /// </summary>
    /// <param name="debugName">リソース名の接頭辞（"RT"/"Depth" を付加）</param>
    void Initialize(const wchar_t* debugName, uint32_t width = 1280, uint32_t height = 720, const Vector4& clearColor = { 0.10f, 0.10f, 0.12f, 1.0f });

    /// <summary>
    /// GPU リソースとディスクリプタを解放する（Renderer の Finalize 後でも安全）
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画先を RENDER_TARGET へ遷移し、OM 設定・クリア・RT サイズのビューポート/シザー設定を行う
    /// </summary>
    /// <param name="postProcess">EndPass でポストエフェクトを掛ける（描画先は一時 RT になる）</param>
    void BeginPass(bool postProcess = false);

    /// <summary>
    /// BeginPass で postProcess を指定していれば stack を掛けてプレビューの RT へ書き、ImGui がサンプルするため PIXEL_SHADER_RESOURCE へ遷移する。
    /// RT/ビューポートの復帰は後段パスに任せる
    /// </summary>
    /// <param name="stack">掛けるポストエフェクト（BeginPass で postProcess を指定したときのみ使う）</param>
    /// <param name="camera">プレビューのカメラ（深度を読むエフェクトが使う）</param>
    void EndPass(const PostProcessStack* stack = nullptr, const Camera* camera = nullptr);

    /// <summary>
    /// 現在の ImGui ウィンドウ残り領域へアスペクト維持レターボックスで中央配置描画する。
    /// 画像は左/右/中ボタンを受けるアイテムとして置くため、直後の IsItemHovered/IsItemActive で操作を判定できる
    /// </summary>
    void DrawImGuiImage() const;

    //============================================================
    //Getter
    //============================================================
    bool IsInitialized() const { return static_cast<bool>(color_.resource); }
    float GetAspect() const { return static_cast<float>(width_) / static_cast<float>(height_); }
    GpuResource& GetDepthResource() { return depthBuffer_; }
    uint32_t GetDepthSrvIndex() const { return depthSrvIndex_; }

  private: //メンバー変数
    DX12Basic*                  dx12_          = nullptr;  ///< Finalize 時に使う（Renderer より後に破棄され得るため生成時に保持）
    RenderTexture               color_;                    ///< ImGui に表示する RT
    RenderTexture*              sceneTarget_   = nullptr;  ///< BeginPass〜EndPass の描画先（color_ か、ポストエフェクト前の一時 RT）
    GpuResource                 depthBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    uint32_t                    dsvIndex_      = 0;
    uint32_t                    depthSrvIndex_ = 0;        ///< デカールが深度を読むための SRV
    uint32_t                    width_         = 0;
    uint32_t                    height_        = 0;
    Vector4                     clearColor_{};
  };

} // namespace Tako

#endif // _DEBUG
