#pragma once
#include "DX12Basic.h"
#include "Vector4.h"
#include "Vector3.h"
#include "Mat4x4Func.h"
#include <memory>
#include "Camera.h"
#include "AABB.h"

// ComPtr のエイリアス
template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace Tako {

  /// <summary>
  /// 3D ワールド空間のライン描画クラス。線分・矢印・球・AABB・OBB・グリッドのワイヤーフレーム描画を提供
  /// </summary>
  class LineRenderer
  {
  private: // シングルトン設定
    static std::unique_ptr<LineRenderer> instance_;  ///< インスタンス

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~LineRenderer() = default;
    LineRenderer(const LineRenderer&) = delete;
    LineRenderer& operator=(const LineRenderer&) = delete;

    friend struct std::default_delete<LineRenderer>;

  public:
    explicit LineRenderer(Token) {}

  private: //定数
    static constexpr uint32_t kLineMaxCount    = 100000;  ///< 線の最大数
    static constexpr uint32_t kVertexCountLine = 2;       ///< 線の頂点数

#ifdef _DEBUG
    static constexpr uint32_t kPreviewLineMaxCount = 8192;  ///< プレビュー用線分の最大数
#endif // _DEBUG

  public: //構造体
    /// <summary>
    /// 頂点データ構造体
    /// </summary>
    struct VertexData
    {
      Vector3 position; ///< 頂点位置
      Vector4 color;    ///< 頂点色
    };

    /// <summary>
    /// 座標変換行列データ
    /// </summary>
    struct TransformationMatrix
    {
      Matrix4x4 WVP; ///< ワールド・ビュー・プロジェクション行列
    };

    /// <summary>
    /// 線分描画用データ構造体
    /// </summary>
    struct LineData
    {
      VertexData* vertexData;                      ///< 頂点データ
      ComPtr<ID3D12Resource> vertexBuffer;         ///< 頂点バッファ
      D3D12_VERTEX_BUFFER_VIEW vertexBufferView;   ///< 頂点バッファビュー
    };

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>LineRenderer のシングルトンインスタンス</returns>
    static LineRenderer* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX12基盤</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// ImGui の描画
    /// </summary>
    void ImGui();

    /// <summary>
    /// 線の描画
    /// </summary>
    /// <param name="start">開始点</param>
    /// <param name="end">終了点</param>
    /// <param name="color">描画色</param>
    void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

    /// <summary>
    /// 矢印の描画（線 + 十字型の先端）
    /// </summary>
    /// <param name="start">矢印の根元</param>
    /// <param name="end">矢印の先端</param>
    /// <param name="color">描画色</param>
    /// <param name="headSize">矢印の先端サイズ</param>
    void DrawArrow(const Vector3& start, const Vector3& end, const Vector4& color, float headSize = 0.2f);

    /// <summary>
    /// 球体の描画
    /// </summary>
    /// <param name="center">中心座標</param>
    /// <param name="radius">半径</param>
    /// <param name="color">描画色</param>
    void DrawSphere(const Vector3& center, const float radius, const Vector4& color, uint32_t subdivision = 10);

    /// <summary>
    /// AABB の描画
    /// </summary>
    /// <param name="aabb">AABB 境界ボックス</param>
    /// <param name="color">描画色</param>
    void DrawAABB(const AABB& aabb, const Vector4& color);

    /// <summary>
    /// OBB の描画
    /// </summary>
    /// <param name="obb">OBB 境界ボックス</param>
    /// <param name="color">描画色</param>
    void DrawOBB(const struct OBB& obb, const Vector4& color);

    /// <summary>
    /// グリッドの描画
    /// </summary>
    /// <param name="size">グリッドのサイズ</param>
    /// <param name="subdivision">分割数</param>
    /// <param name="color">描画色</param>
    void DrawGrid(const float size, const float subdivision, const Vector4& color);

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    /// <summary>
    /// リセット
    /// </summary>
    void Reset();

#ifdef _DEBUG
    /// <summary>
    /// 以降の DrawLine 系呼び出しをプレビュー用バッファへ振り向ける
    /// </summary>
    void BeginPreviewLines() { previewBatchMode_ = true; }

    /// <summary>
    /// プレビュー用バッファへの振り向けを解除する
    /// </summary>
    void EndPreviewLines() { previewBatchMode_ = false; }

    /// <summary>
    /// 溜めたプレビュー線分を指定 VP で描画してバッファをリセットする。
    /// RT/ビューポートは呼び出し側で設定済みであること。CB 1本を使い回すため 1 フレーム 1 視点まで
    /// </summary>
    /// <param name="viewProjection">プレビューカメラのビュープロジェクション行列</param>
    void DrawPreviewLines(const Matrix4x4& viewProjection);
#endif // _DEBUG

    //============================
    //Setter
    //============================
    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetDebug(bool isDebug) { isDebug_ = isDebug; }

    //============================
    //Getter
    //============================
    bool GetDebug() const { return isDebug_; }

  private: //非公開関数
    /// <summary>
    /// ルートシグネチャの作成
    /// </summary>
    void CreateRootSignature();

    /// <summary>
    /// パイプラインステートの生成
    /// </summary>
    void CreatePSO();

    /// <summary>
    /// 線の頂点データを生成
    /// </summary>
    /// <param name="lineData">線データ</param>
    /// <param name="lineCount">確保する線分数</param>
    void CreateLineVertexData(LineData* lineData, uint32_t lineCount = kLineMaxCount);

    /// <summary>
    /// 座標変換行列データを生成
    /// </summary>
    void CreateTransformMatData();

  private: //メンバー変数

    DX12Basic* dx12_;  ///< DX12Basic クラスのインスタンス

    Camera* camera_;  ///< カメラ

    bool isDebug_;  ///< デバッグフラグ

    uint32_t lineIndex_ = 0;  ///< 線の頂点書き込みカーソル

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;  ///< ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;  ///< パイプラインステート

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixBuffer_;  ///< 座標変換行列バッファ

    TransformationMatrix* transformationMatrixData_;  ///< 座標変換行列データ

    std::unique_ptr<LineData> lineData_;  ///< 線データ

#ifdef _DEBUG
    //エディタプレビュー用線分描画
    std::unique_ptr<LineData>              previewLineData_;                              ///< プレビュー専用頂点バッファ（メインバッチと GPU 実行前の上書き競合を避けるため分離）
    Microsoft::WRL::ComPtr<ID3D12Resource> previewTransformationMatrixBuffer_;
    TransformationMatrix*                  previewTransformationMatrixData_   = nullptr;
    uint32_t                               previewLineIndex_                  = 0;
    bool                                   previewBatchMode_                  = false;    ///< true 中は DrawLine 系がプレビューバッファへ書く
#endif // _DEBUG
  };

} // namespace Tako
