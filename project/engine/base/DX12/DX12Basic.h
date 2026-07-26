#pragma once
#include "WinApp.h"
#include <array>
#include<string>
#include<fstream>
#include<sstream>
#include<chrono>
#include<wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include<dxcapi.h>
#include "DirectXTex.h"
#include "DirectXTex.inl"
#include <unordered_map>

#include"Vector4.h"

namespace Tako {

  /// <summary>
  /// DirectX 12基盤システムクラス
  /// デバイス、コマンドキュー、スワップチェーン管理
  /// </summary>
  class DX12Basic {
  public: //構造体
    /// <summary>
    /// ComPtr のエイリアス
    /// </summary>
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  public: //メンバー関数

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~DX12Basic();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="winApp">ウィンドウ管理クラスのポインタ</param>
    void Initialize(WinApp* winApp);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// renderTexture を設定
    /// </summary>
    void SetEffectRenderTexture();

    /// <summary>
    /// エフェクトなしレンダーテクスチャを設定
    /// </summary>
    void SetNonEffectRenderTexture();

    /// <summary>
    /// swapChain を設定
    /// </summary>
    void SetSwapChain();

    /// <summary>
    /// 描画後の処理
    /// </summary>
    void EndDraw();

    /// <summary>
    /// コマンド完了まで待機
    /// </summary>
    void WaitForGPU();

    /// <summary>
    /// シェーダーをコンパイル
    /// </summary>
    /// <param name="filePath">シェーダーファイルのパス</param>
    /// <param name="profile">シェーダープロファイル（例: vs_6_0, ps_6_0）</param>
    /// <returns>コンパイル済みシェーダーのバイナリ</returns>
    ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

    /// <summary>
    /// バッファリソースの生成
    /// </summary>
    /// <param name="sizeInBytes">バッファサイズ（バイト）</param>
    /// <returns>生成されたバッファリソース</returns>
    ComPtr<ID3D12Resource> MakeBufferResource(size_t sizeInBytes);

    /// <summary>
    /// バッファリソースの生成（参照版）
    /// </summary>
    /// <param name="bufferResource">出力先のバッファリソース</param>
    /// <param name="sizeInBytes">バッファサイズ（バイト）</param>
    void CreateBufferResource(ComPtr<ID3D12Resource>& bufferResource, size_t sizeInBytes);

    /// <summary>
    /// UAV リソースの生成
    /// </summary>
    /// <param name="uavResource">出力先の UAV リソース</param>
    /// <param name="sizeInBytes">リソースサイズ（バイト）</param>
    void CreateResourceForUAV(ComPtr<ID3D12Resource>& uavResource, UINT sizeInBytes);

    /// <summary>
    /// テクスチャリソースの生成
    /// </summary>
    /// <param name="metaData">テクスチャのメタデータ（サイズ、フォーマット等）</param>
    /// <returns>生成されたテクスチャリソース</returns>
    ComPtr<ID3D12Resource> MakeTextureResource(const DirectX::TexMetadata& metaData);

    /// <summary>
    /// テクスチャリソースの生成（参照版）
    /// </summary>
    /// <param name="textureResource">出力先のテクスチャリソース</param>
    /// <param name="metaData">テクスチャのメタデータ（サイズ、フォーマット等）</param>
    void CreateTextureResource(ComPtr<ID3D12Resource>& textureResource, const DirectX::TexMetadata& metaData);

    /// <summary>
    /// レンダーテクスチャリソースの生成
    /// </summary>
    /// <param name="rendertextureResource">出力先のレンダーテクスチャリソース</param>
    /// <param name="width">テクスチャの幅（ピクセル）</param>
    /// <param name="height">テクスチャの高さ（ピクセル）</param>
    /// <param name="format">ピクセルフォーマット（例: DXGI_FORMAT_R8G8B8A8_UNORM）</param>
    /// <param name="clearColor">クリアカラー（RGBA、各要素0.0-1.0）</param>
    void CreateRenderTextureResource(ComPtr<ID3D12Resource>& rendertextureResource, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

    /// <summary>
    /// テクスチャリソースの転送
    /// </summary>
    /// <param name="texture">転送先のテクスチャリソース</param>
    /// <param name="mipImages">転送するミップマップ画像データ</param>
    /// <returns>アップロード用の中間バッファリソース（転送完了まで保持必要）</returns>
    [[nodiscard]]
    ComPtr<ID3D12Resource> UploadTextureData(const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

    /// <summary>
    /// テクスチャファイルの読み込み
    /// </summary>
    /// <param name="filePath">テクスチャファイルのパス（.png, .jpg 等）</param>
    /// <returns>読み込まれた画像データ</returns>
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    /// <summary>
    /// トランジションバリアの設定
    /// </summary>
    /// <param name="stateBefore">遷移前のリソース状態</param>
    /// <param name="stateAfter">遷移後のリソース状態</param>
    /// <param name="resource">対象リソース</param>
    void TransitionResourceState(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, ID3D12Resource* resource);

    /// <summary>
    /// リソース状態追跡付きバリア遷移
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="newState">新しいリソース状態</param>
    void TransitionResourceWithTracking(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState);

    /// <summary>
    /// リソースの初期状態を設定（バリア遷移なし）
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="initialState">初期状態</param>
    void SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);

    /// <summary>
    /// リソースの状態追跡エントリを削除（リソースを破棄/再作成する前に呼ぶ）
    /// 解放済みポインタのエントリが残り、アドレス再利用時に誤った状態遷移を起こすのを防ぐ。
    /// </summary>
    /// <param name="resource">対象リソース</param>
    void RemoveResourceState(ID3D12Resource* resource);

    /// <summary>
    /// UAV リソースバリアの設定
    /// </summary>
    /// <param name="resource">対象 UAV リソース</param>
    void SetUAVBarrier(ID3D12Resource* resource);

    /// <summary>
    /// ビューポートとシザリング矩形をセット
    /// </summary>
    void SetViewPort();

    /// <summary>
    /// RTV,DepthBuffer のリサイズ
    /// </summary>
    /// <param name="width">新しい幅（ピクセル）</param>
    /// <param name="height">新しい高さ（ピクセル）</param>
    void ResizeBuffers(uint32_t width, uint32_t height);

    /// <summary>
    /// ビューポートとシザー矩形の更新
    /// </summary>
    void UpdateViewportAndScissorRect() {
      InitViewport();
      InitScissorRect();
    }

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 現在のリソース状態を取得
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <returns>現在のリソース状態</returns>
    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource) const;

    ID3D12Device* GetDevice() {
      return device_.Get();
    }

    ID3D12GraphicsCommandList* GetCommandList() {
      return commandList_.Get();
    }

    ID3D12CommandQueue* GetCommandQueue() {
      return commandQueue_.Get();
    }

    size_t GetSwapChainBufferCount() {
      return swapChainResources_.size();
    }

    /// <summary>
    /// メイン深度バッファの DSV ハンドルの取得
    ///	</summary>
    /// <returns>メイン DSV の CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetMainDSVHandle() const;

    ID3D12Resource* GetDepthStencilResource() {
      return depthStencilResource_.Get();
    }

    IDXGISwapChain4* GetSwapChain() {
      return swapChain_.Get();
    }

    /// <summary>
    /// スワップチェインの RTVHandle を取得
    ///	</summary>
    /// <returns>現在のバックバッファの RTV ハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetSwapChainRTVHandle() {
      UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
      return rtvHandle_[backBufferIndex];
    }

    D3D12_VIEWPORT GetViewport() {
      return viewport_;
    }

    D3D12_RECT GetScissorRect() {
      return scissorRect_;
    }
  private: //非公開関数
    /// <summary>
    /// device の初期化
    /// </summary>
    void InitDevice();

    /// <summary>
    /// コマンド関連の初期化
    /// </summary>
    void InitCommand();

    /// <summary>
    /// スワップチェインの生成
    /// </summary>
    void CreateSwapChain();

    /// <summary>
    /// 深度バッファの生成
    /// </summary>
    void CreateDepthStencilResource();

    /// <summary>
    /// デスクリプタヒープの初期化
    /// </summary>
    void InitDescriptorHeap();

    /// <summary>
    /// レンダーターゲットビューの初期化
    /// </summary>
    void InitSwapChainRTV();

    /// <summary>
    /// 深度ステンシルビューの初期化
    /// </summary>
    void InitDSV();

    /// <summary>
    /// フェンスの初期化
    /// </summary>
    void InitFence();

    /// <summary>
    ///　ビューポート矩形の初期化
    /// </summary>
    void InitViewport();

    /// <summary>
    /// シザリング矩形の初期化
    /// </summary>
    void InitScissorRect();

    /// <summary>
    /// DXC コンパイラの生成
    /// </summary>
    void CreateDXCCompiler();

    /// <summary>
    /// FPS 制御初期化
    /// </summary>
    void InitFPSLimiter();

    /// <summary>
    /// FPS 制御更新
    /// </summary>
    void UpdateFPSLimiter();

    /// <summary>
    /// 深度バッファの再作成
    /// </summary>
    void RecreateDepthBuffer();

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="descriptorHeap">デスクリプタヒープ</param>
    /// <param name="descriptorSize">デスクリプタのサイズ（バイト）</param>
    /// <param name="index">インデックス番号</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

    /// <summary>
    /// 指定番号の GPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="descriptorHeap">デスクリプタヒープ</param>
    /// <param name="descriptorSize">デスクリプタのサイズ（バイト）</param>
    /// <param name="index">インデックス番号</param>
    /// <returns>GPU ディスクリプタハンドル</returns>
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

    /// <summary>
    /// バックバッファのバリアを設定
    /// </summary>
    /// <param name="stateBefore">遷移前のリソース状態</param>
    /// <param name="stateAfter">遷移後のリソース状態</param>
    void SetBackBufferBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

  private: //メンバー変数

    std::chrono::steady_clock::time_point referenceTime_;  ///< 記録時間（FPS 制御用の基準時刻）

    static const UINT kRtvHandleCount = 2;  ///< RTV ハンドルの要素数（スワップチェイン用バックバッファ数）

    mutable std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> resourceStates_;  ///< リソース状態追跡用マップ（バリア遷移の最適化に使用）

    WinApp* winApp_ = nullptr;  ///< ウィンドウクラスポインター（ウィンドウサイズ等の取得に使用）

    ComPtr<ID3D12Device> device_;  ///< DirectX 12デバイス（リソース生成の中心オブジェクト）

    ComPtr<IDXGIFactory7> dxgiFactory_;  ///< DXGI ファクトリ（スワップチェイン生成に使用）

    ComPtr<ID3D12CommandQueue> commandQueue_;  ///< コマンドキュー（GPU 実行キュー）

    ComPtr<ID3D12CommandAllocator> commandAllocator_;  ///< コマンドアロケータ（コマンドリストのメモリ管理）

    ComPtr<ID3D12GraphicsCommandList> commandList_;  ///< コマンドリスト（描画コマンドの記録）

    ComPtr<IDXGISwapChain4> swapChain_;  ///< スワップチェイン（ダブルバッファリング管理）

    ComPtr<ID3D12Resource> depthStencilResource_;  ///< 深度バッファリソース（深度テスト用）

    uint32_t mainDsvIndex_ = 0;  ///< メイン深度バッファの DSV インデックス（DsvManager 発行）

    std::array<ComPtr<ID3D12Resource>, 2> swapChainResources_;  ///< スワップチェインのバッファ（ダブルバッファリング用2枚）

    UINT swapChainBufferCount_;  ///< スワップチェインのバッファのカウント（通常2）

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kRtvHandleCount];  ///< RTV ハンドル配列（各バックバッファ用）

    uint32_t swapChainRtvIndices_[kRtvHandleCount] = {};  ///< スワップチェーン用 RTV インデックス（RtvManager 発行）

    ComPtr<ID3D12Fence> fence_;  ///< フェンスオブジェクト（GPU 同期用）

    HANDLE fenceEvent_;  ///< フェンスイベントハンドル（CPU 待機用）

    UINT64 fenceValue_;  ///< フェンスの値（同期カウンター）

    D3D12_VIEWPORT viewport_;  ///< ビューポート（描画領域の定義）

    D3D12_RECT scissorRect_;  ///< シザリング矩形（描画範囲の制限）

    ComPtr<IDxcUtils> dxcUtils_ = nullptr;  ///< DXC ユーティリティ（シェーダーコンパイル補助）

    ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;  ///< DXC コンパイラ（HLSL → DXIL 変換）

    ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;  ///< デフォルトインクルードハンドラー（シェーダーファイルのインクルード処理）

  };

} // namespace Tako
