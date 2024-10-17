#pragma once
#include "DX12Basic.h"
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include<vector>

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
		Vector2 position;
	};

	struct ColorData
	{
		Vector4 color;
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
	};

	// 三角形構造体
	struct TriangleData
	{
		VertexData* vertexData;
		Vector4* color;
		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexBuffer;
		// カラーバッファ
		ComPtr<ID3D12Resource> colorBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	};

	// 線分構造体
	struct LineData
	{
		VertexData* vertexData;
		Vector4* color;
		float weight;
		// 頂点バッファ
		ComPtr<ID3D12Resource> vertexBuffer;
		// カラーバッファ
		ComPtr<ID3D12Resource> colorBuffer;
		// 頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
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
	void DrawTriangle(const Vector2& pos1, const Vector2& pos2, const Vector2& pos3, const Vector4& color);

	/// <summary>
	/// 線の描画
	/// </summary>
	void DrawLine(const Vector2& start, const Vector2& end, const Vector4& color);

private: // プライベートメンバ関数
	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState);

	///<summary>
	/// 三角形の頂点データを生成
	/// </summary>
	void CreateTriangleVertexData(TriangleData* triangleData);

	///<summary>
	/// 線の頂点データを生成
	/// </summary>
	void CreateLineVertexData(LineData* lineData);

	///<summary>
	/// カラーデータを生成
	/// </summary>
	void CreateColorData(TriangleData* triangleData);
	void CreateColorData(LineData* lineData);

	///<summary>
	/// 座標変換行列データを生成
	/// </summary>
	void CreateTransformMatData();

	///<summary>
	/// 三角形データの初期化
	/// </summary>
	void InitializeTriangleData(TriangleData* triangleData);

	///<summary>
	/// 線データの初期化
	/// </summary>
	void InitializeLineData(LineData* lineData);

	/// <summary>
	/// 共通描画設定
	/// <summary>
	void SetCommonRenderSetting();

private: // メンバ変数

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> trianglePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> linePipelineState_;

	// 座標変換行列バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixBuffer_;

	// 座標変換行列データ
	TransformationMatrix* transformationMatrixData_;

	// 三角形データ
	vector<TriangleData*> triangleDatas_;

	// 線データ
	vector<LineData*> lineDatas_;

};