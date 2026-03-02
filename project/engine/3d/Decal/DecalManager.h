#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <list>

#include "Camera.h"
#include "Matrix4x4.h"

namespace Tako {

  class DX12Basic;
  class Camera;
  class Decal;

  /// <summary>
  /// Projective Decal 描画の基盤システムクラス
  /// シングルトンパターンで実装
  /// PSO / RootSignature / 単位キューブメッシュ / 深度SRV を管理
  /// </summary>
  class DecalManager {
  private: // シングルトン設定

    static std::unique_ptr<DecalManager> instance_;

    DecalManager() = default;
    ~DecalManager() = default;
    DecalManager(DecalManager&) = delete;
    DecalManager& operator=(DecalManager&) = delete;

    friend struct std::default_delete<DecalManager>;

  public: // メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    static DecalManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX 12基盤システムのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// ウィンドウリサイズ時の処理（深度SRVの再作成）
    /// </summary>
    void OnResize();

    /// <summary>
    /// デカール描画パスの開始
    /// 深度バッファを PIXEL_SHADER_RESOURCE に遷移、RTV を DSV なしで再バインド
    /// </summary>
    void BeginDraw();

    /// <summary>
    /// デカール描画パスの終了
    /// 深度バッファを DEPTH_WRITE に復帰、RTV + DSV を再バインド
    /// </summary>
    void EndDraw();

    /// <summary>
    /// デカールを登録
    /// </summary>
    void AddDecal(Decal* decal);

    /// <summary>
    /// デカールの登録を解除
    /// </summary>
    void RemoveDecal(Decal* decal);

    /// <summary>
    /// 登録済み全デカールの更新
    /// </summary>
    void UpdateAll();

    /// <summary>
    /// 登録済み全デカールの描画（BeginDraw/EndDraw を内部で呼び出す）
    /// </summary>
    void DrawAll();

    /// <summary>
    /// 登録済み全デカールのデバッグ描画
    /// </summary>
    void DrawAllDebug();

    /// <summary>
    /// 全デカールの登録を解除（シーン切替時）
    /// </summary>
    void ClearDecals();

    // ===== Getters =====
    /// <summary>
    /// DirectX12基盤システムを取得
    /// </summary>
    DX12Basic* GetDX12Basic() const { return m_dx12_; }

    /// <summary>
    /// カメラのポインタを取得
    /// </summary>
    Camera* GetCamera() const { return camera_; }

    /// <summary>
    /// ビュープロジェクション行列を取得
    /// </summary>
    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

    /// <summary>
    /// キューブメッシュの頂点バッファビューを取得
    /// </summary>
    const D3D12_VERTEX_BUFFER_VIEW& GetCubeVBV() const { return cubeVBV_; }

    /// <summary>
    /// キューブメッシュのインデックスバッファビューを取得
    /// </summary>
    const D3D12_INDEX_BUFFER_VIEW& GetCubeIBV() const { return cubeIBV_; }

    // ===== Setters =====
    /// <summary>
    /// カメラを設定
    /// </summary>
    void SetCamera(Camera* camera) { camera_ = camera; }

  private: // プライベートメンバー関数

    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// パイプラインステートの生成
    /// </summary>
    void CreatePSO();

    /// <summary>
    /// 単位キューブメッシュの作成
    /// </summary>
    void CreateCubeMesh();

    /// <summary>
    /// ViewData 定数バッファの作成
    /// </summary>
    void CreateViewDataBuffer();

    /// <summary>
    /// 深度 SRV の作成
    /// </summary>
    void CreateDepthSRV();

  private: // メンバー変数

    DX12Basic* m_dx12_ = nullptr; ///< DirectX12基盤システムへの参照

    Camera* camera_ = nullptr; ///< カメラへのポインタ

    Matrix4x4 viewProjectionMatrix_; ///< ビュープロジェクション行列

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; ///< ルートシグネチャ

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; ///< パイプラインステート

    // 単位キューブメッシュ
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeVertexBuffer_; ///< キューブ頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeIndexBuffer_;  ///< キューブインデックスバッファ
    D3D12_VERTEX_BUFFER_VIEW cubeVBV_{};  ///< 頂点バッファビュー
    D3D12_INDEX_BUFFER_VIEW cubeIBV_{};   ///< インデックスバッファビュー

    // ViewData 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> viewDataBuffer_; ///< ViewData 定数バッファリソース

    /// <summary>
    /// GPU に送る ViewData 構造体
    /// </summary>
    struct ViewDataGPU {
      Matrix4x4 invViewProj;
      float screenWidth;
      float screenHeight;
      float padding[2];
    };
    ViewDataGPU* viewDataMapped_ = nullptr; ///< マップ済みポインタ

    // 深度 SRV
    uint32_t depthSrvIndex_ = 0; ///< 深度テクスチャの SRV インデックス

    std::list<Decal*> decals_; ///< 登録済みデカールリスト
  };

} // namespace Tako
