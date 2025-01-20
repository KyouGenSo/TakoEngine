#pragma once
#include "DX12Basic.h"
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include<vector>
#include <list>
#include "Camera.h"

using namespace std;

// ComPtrのエイリアス
template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

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
	struct VertexData
	{
		Vector3 position;
		Vector4 color;
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
	};

	// 三角形構造体
	struct TriangleData
	{
		VertexData* vertexData;
		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	};

	// 矩形構造体
	struct BoxData
	{
		VertexData* vertexData;
		uint32_t* indexData;
		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexBuffer;
		// インデックスバッファ
		ComPtr<ID3D12Resource> indexBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		// インデックスバッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
	};

	// 線分構造体
	struct LineData
	{
		VertexData* vertexData;
		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	};

	struct Sphere {
		Vector3 center;
		float radius;
	};

public: // メンバ関数

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static Draw2D* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
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
	void DrawTriangle(const Vector3& pos1, const Vector3& pos2, const Vector3& pos3, const Vector4& color);

	/// <summary>
	/// 矩形の描画
	/// </summary>
	void DrawBox(const Vector3& pos, const Vector3& size, const Vector4& color);
	void DrawBox(const Vector3& pos, const Vector3& size, const float angle, const Vector4& color);

	/// <summary>
	/// 線の描画
	/// </summary>
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

	void DrawSphere(const Vector3& center, const float radius, const Vector4& color);

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
	/// デバッグ用ビューマトリックスを取得
	/// <summary>
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const bool GetDebug() const { return isDebug_; }


	// -----------------------------------Setters-----------------------------------//
	/// <summary>
	/// プロジェクションマトリックスを設定
	/// <summary>
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }
	void SetCamera(Camera* camera) { m_camera_ = camera; }
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }
	

private: // プライベートメンバ関数
	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSignature);

	/// <summary>
	/// 三角形の頂点データを生成
	/// </summary>
	void CreateTriangleVertexData(TriangleData* triangleData);

	/// <summary>
	/// 矩形の頂点データを生成
	/// </summary>
	void CreateBoxVertexData(BoxData* boxData);

	///<summary>
	/// 線の頂点データを生成
	/// </summary>
	void CreateLineVertexData(LineData* lineData);

	///<summary>
	/// 座標変換行列データを生成
	/// </summary>
	void CreateTransformMatData();

	/// <summary>
	/// 球の頂点位置を計算
	/// </summary>
	void CalcSphereVertexData();

private: // メンバ変数

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_;

	Camera* m_camera_;

	bool isDebug_;

	const uint32_t kTrriangleMaxCount = 30096;
	const uint32_t kVertexCountTrriangle = 3;

	const uint32_t kBoxMaxCount = 30096;
	const uint32_t kVertexCountBox = 4;
	const uint32_t kIndexCountBox = 6;

	const uint32_t kLineMaxCount = 100000;
	const uint32_t kVertexCountLine = 2;

	// 三角形のインデクス
	uint32_t triangleIndex_ = 0;

	// 矩形のインデクス
	uint32_t boxIndexIndex_ = 0;
	uint32_t boxVertexIndex_ = 0;

	// 線のインデクス
	uint32_t lineIndex_ = 0;

	// マトリックス
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewPortMatrix_;

	Matrix4x4 debugViewMatrix_;
	Matrix4x4 debugProjectionMatrix_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> triangleRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> lineRootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> trianglePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> linePipelineState_;

	// 座標変換行列バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixBuffer_;

	// 座標変換行列データ
	TransformationMatrix* transformationMatrixData_;

	// 三角形データ
	TriangleData* triangleData_;

	// 矩形データ
	BoxData* boxData_;

	// 線データ
	LineData* lineData_;

	// 球のデータ
	std::vector<Vector3> spheres_;
};