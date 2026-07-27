#pragma once

#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "Vector4.h"

namespace Tako {

  class DX12Basic;

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
    /// GPU リソースとディスクリプタを解放する（Object3dBasic の Finalize 後でも安全）
    /// </summary>
    void Finalize();

    /// <summary>
    /// RT を RENDER_TARGET へ遷移し、OM 設定・クリア・RT サイズのビューポート/シザー設定を行う
    /// </summary>
    void BeginPass();

    /// <summary>
    /// ImGui がサンプルするため RT を PIXEL_SHADER_RESOURCE へ遷移する。RT/ビューポートの復帰は後段パスに任せる
    /// </summary>
    void EndPass();

    /// <summary>
    /// 現在の ImGui ウィンドウ残り領域へアスペクト維持レターボックスで中央配置描画する
    /// </summary>
    /// <returns>画像上にカーソルがある場合 true</returns>
    bool DrawImGuiImage() const;

    //============================================================
    //Getter
    //============================================================
    bool IsInitialized() const { return renderTexture_ != nullptr; }
    float GetAspect() const { return static_cast<float>(width_) / static_cast<float>(height_); }

  private: //メンバー変数
    DX12Basic*                             dx12_          = nullptr;  ///< Finalize 時に使う（Object3dBasic より後に破棄され得るため生成時に保持）
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE            rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE            dsvHandle_{};
    uint32_t                               rtvIndex_      = 0;
    uint32_t                               dsvIndex_      = 0;
    uint32_t                               srvIndex_      = 0;
    uint32_t                               width_         = 0;
    uint32_t                               height_        = 0;
    Vector4                                clearColor_{};
  };

} // namespace Tako

#endif // _DEBUG
