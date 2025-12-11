#pragma once
#include "DX12Basic.h"
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include <vector>
#include <list>
#include <memory>
#include "Camera.h"
#include "AABB.h"

using namespace std;

// ComPtrのエイリアス
template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace Tako {

/// <summary>
/// 2Dプリミティブ描画クラス。三角形、矩形、線、球、AABB、OBB、グリッドなどのデバッグ描画を提供
/// </summary>
class Draw2D
{
private: // シングルトン設定
	// インスタンス
	static Draw2D* instance_;

	Draw2D() = default;
	~Draw2D() = default;
	Draw2D(const Draw2D&) = delete;
	Draw2D& operator=(const Draw2D&) = delete;

public: // 構造体
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
	/// 三角形描画用データ構造体
	/// </summary>
	struct TriangleData
	{
		VertexData* vertexData;                      ///< 頂点データ
		ComPtr<ID3D12Resource> vertexBuffer;         ///< 頂点バッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;   ///< 頂点バッファビュー
	};

	/// <summary>
	/// 矩形描画用データ構造体
	/// </summary>
	struct BoxData
	{
		VertexData* vertexData;                      ///< 頂点データ
		uint32_t* indexData;                         ///< インデックスデータ
		ComPtr<ID3D12Resource> vertexBuffer;         ///< 頂点バッファ
		ComPtr<ID3D12Resource> indexBuffer;          ///< インデックスバッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;   ///< 頂点バッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView;     ///< インデックスバッファビュー
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

	/// <summary>
	/// 球体データ構造体
	/// </summary>
	struct Sphere {
		Vector3 center; ///< 中心座標
		float radius;   ///< 半径
	};

public: // メンバ関数

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	/// <returns>Draw2Dのシングルトンインスタンス</returns>
	static Draw2D* GetInstance();

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
	/// ImGuiの描画
	/// </summary>
	void ImGui();

	/// <summary>
	/// 三角形の描画
	/// </summary>
	/// <param name="pos1">頂点1の位置</param>
	/// <param name="pos2">頂点2の位置</param>
	/// <param name="pos3">頂点3の位置</param>
	/// <param name="color">描画色</param>
	void DrawTriangle(const Vector3& pos1, const Vector3& pos2, const Vector3& pos3, const Vector4& color);

	/// <summary>
	/// 矩形の描画
	/// </summary>
	/// <param name="pos">矩形の位置</param>
	/// <param name="size">矩形のサイズ</param>
	/// <param name="color">描画色</param>
	void DrawBox(const Vector3& pos, const Vector3& size, const Vector4& color);

	/// <summary>
	/// 矩形の描画（回転付き）
	/// </summary>
	/// <param name="pos">矩形の位置</param>
	/// <param name="size">矩形のサイズ</param>
	/// <param name="angle">回転角度（ラジアン）</param>
	/// <param name="color">描画色</param>
	void DrawBox(const Vector3& pos, const Vector3& size, const float angle, const Vector4& color);

	/// <summary>
	/// 線の描画
	/// </summary>
	/// <param name="start">開始点</param>
	/// <param name="end">終了点</param>
	/// <param name="color">描画色</param>
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

	/// <summary>
	/// 球体の描画
	/// </summary>
	/// <param name="center">中心座標</param>
	/// <param name="radius">半径</param>
	/// <param name="color">描画色</param>
	void DrawSphere(const Vector3& center, const float radius, const Vector4& color);

	/// <summary>
	/// AABBの描画
	/// </summary>
	/// <param name="aabb">AABB境界ボックス</param>
	/// <param name="color">描画色</param>
	void DrawAABB(const AABB& aabb, const Vector4& color);

	/// <summary>
	/// OBBの描画
	/// </summary>
	/// <param name="obb">OBB境界ボックス</param>
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

	// -----------------------------------Getters-----------------------------------//
	/// <summary>
	/// プロジェクションマトリックスを取得
	/// </summary>
	/// <returns>プロジェクション行列</returns>
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

	/// <summary>
	/// デバッグ状態を取得
	/// </summary>
	/// <returns>デバッグが有効な場合true</returns>
	const bool GetDebug() const { return isDebug_; }


	// -----------------------------------Setters-----------------------------------//
	/// <summary>
	/// プロジェクションマトリックスを設定
	/// </summary>
	/// <param name="projectionMatrix">設定するプロジェクション行列</param>
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	/// <summary>
	/// カメラを設定
	/// </summary>
	/// <param name="camera">設定するカメラ</param>
	void SetCamera(Camera* camera) { m_camera_ = camera; }

	/// <summary>
	/// デバッグ状態を設定
	/// </summary>
	/// <param name="isDebug">デバッグフラグ</param>
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }
	

private: // プライベートメンバ関数
	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	/// <param name="rootSignature">作成するルートシグネチャ</param>
	void CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	/// <param name="primitiveTopologyType">プリミティブトポロジタイプ</param>
	/// <param name="pipelineState">作成するパイプラインステート</param>
	/// <param name="rootSignature">使用するルートシグネチャ</param>
	void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSignature);

	/// <summary>
	/// 三角形の頂点データを生成
	/// </summary>
	/// <param name="triangleData">三角形データ</param>
	void CreateTriangleVertexData(TriangleData* triangleData);

	/// <summary>
	/// 矩形の頂点データを生成
	/// </summary>
	/// <param name="boxData">矩形データ</param>
	void CreateBoxVertexData(BoxData* boxData);

	/// <summary>
	/// 線の頂点データを生成
	/// </summary>
	/// <param name="lineData">線データ</param>
	void CreateLineVertexData(LineData* lineData);

	/// <summary>
	/// 座標変換行列データを生成
	/// </summary>
	void CreateTransformMatData();

	/// <summary>
	/// 球の頂点位置を計算
	/// </summary>
	void CalcSphereVertexData();

	/// <summary>
	/// グリッドの頂点位置を計算
	/// </summary>
	void CalcGridVertexData();

private: // メンバ変数

	DX12Basic* m_dx12_; ///< DX12Basicクラスのインスタンス

	Camera* m_camera_; ///< カメラ

	bool isDebug_; ///< デバッグフラグ

	const uint32_t kTrriangleMaxCount = 30096; ///< 三角形の最大数
	const uint32_t kVertexCountTrriangle = 3; ///< 三角形の頂点数

	const uint32_t kBoxMaxCount = 30096; ///< 矩形の最大数
	const uint32_t kVertexCountBox = 4; ///< 矩形の頂点数
	const uint32_t kIndexCountBox = 6; ///< 矩形のインデックス数

	const uint32_t kLineMaxCount = 100000; ///< 線の最大数
	const uint32_t kVertexCountLine = 2; ///< 線の頂点数

	uint32_t triangleIndex_ = 0; ///< 三角形のインデクス

	uint32_t boxIndexIndex_ = 0; ///< 矩形のインデックスインデクス
	uint32_t boxVertexIndex_ = 0; ///< 矩形の頂点インデクス

	uint32_t lineIndex_ = 0; ///< 線のインデクス

	Matrix4x4 projectionMatrix_; ///< プロジェクション行列
	Matrix4x4 viewPortMatrix_; ///< ビューポート行列

	Matrix4x4 debugViewMatrix_; ///< デバッグビュー行列
	Matrix4x4 debugProjectionMatrix_; ///< デバッグプロジェクション行列

	Microsoft::WRL::ComPtr<ID3D12RootSignature> triangleRootSignature_; ///< 三角形用ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> lineRootSignature_; ///< 線用ルートシグネチャ

	Microsoft::WRL::ComPtr<ID3D12PipelineState> trianglePipelineState_; ///< 三角形用パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> linePipelineState_; ///< 線用パイプラインステート

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixBuffer_; ///< 座標変換行列バッファ

	TransformationMatrix* transformationMatrixData_; ///< 座標変換行列データ

	std::unique_ptr<TriangleData> triangleData_; ///< 三角形データ

	std::unique_ptr<BoxData> boxData_; ///< 矩形データ

	std::unique_ptr<LineData> lineData_; ///< 線データ

	std::vector<Vector3> sphereVerties_; ///< 球の頂点データ

	std::vector<Vector3> gridVerties_; ///< グリッドの頂点データ
};

} // namespace Tako