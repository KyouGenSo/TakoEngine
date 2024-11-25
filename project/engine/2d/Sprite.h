#pragma once
#include"Vector4.h"
#include"Vector2.h"
#include"Vector3.h"
#include"Mat4x4Func.h"
#include "Transform.h"
#include<cstdint>
#include<string>
#include <d3d12.h>
#include<wrl.h>

class Sprite {
private: // 構造体
	// 頂点データ
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
	};

	// マテリアルデータ
	struct Material
	{
		Vector4 color;
		Matrix4x4 uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
	};

public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	///<summary>
	///初期化
	/// </summary>
	void Initialize(std::string texturePath);

	///<summary>
	///更新
	/// </summary>
	void Update();

	///<summary>
	///描画
	/// </summary>
	void Draw();

	//-----------------------------------Getters-----------------------------------//
	// Get Transform
	const Transform& GetTransform() const { return transform_; }
	// Get MaterialData
	const Material* GetMaterialData() const { return materialData_; }
	// Get Color
	const Vector4& GetColor() const { return materialData_->color; }
	// Get Position
	const Vector2& GetPos() const { return pos_; }
	// Get Rotation
	const float GetRotation() const { return rotation_; }
	// Get Size
	const Vector2& GetSize() const { return size_; }
	// Get AnchorPoint
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	// Get isFlipX
	const bool GetIsFlipX() const { return isFlipX_; }
	// Get isFlipY
	const bool GetIsFlipY() const { return isFlipY_; }
	// Get TexLeftTop
	const Vector2& GetTexLeftTop() const { return texTopLeft_; }
	// Get TexCutSize
	const Vector2& GetTexCutSize() const { return texCutSize_; }

	//-----------------------------------Setters-----------------------------------//
	// Set Transform
	void SetTransform(const Transform& transform) { transform_ = transform; }
	// Set Color
	void SetColor(const Vector4& color) { materialData_->color = color; }
	// Set Position
	void SetPos(const Vector2& pos) { pos_ = pos; }
	// Set Rotation
	void SetRotation(const float rotation) { rotation_ = rotation; }
	// Set Size
	void SetSize(const Vector2& size) { size_ = size; }
	// Set AnchorPoint
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
	// Set isFlipX
	void SetIsFlipX(const bool isFlipX) { isFlipX_ = isFlipX; }
	// Set isFlipY
	void SetIsFlipY(const bool isFlipY) { isFlipY_ = isFlipY; }
	// Set TexLeftTop
	void SetTexLeftTop(const Vector2& texTopLeft) { texTopLeft_ = texTopLeft; }
	// Set TexCutSize
	void SetTexCutSize(const Vector2& texCutSize) { texCutSize_ = texCutSize; }

private: // プライベートメンバー関数

	///<summary>
	///頂点データを生成
	/// </summary>
	void CreateVertexData();

	///<summary>
	///マテリアルデータを生成
	/// </summary>
	void CreateMaterialData();

	/// <summary>
	/// 座標変換行列データを生成
	/// </summary>
	void CreateTransformationMatrixData();

	///　<summary>
	///　画像切り取り範囲をぴったりにする
	/// </summary>
	void FitTexCutSize();

private:// メンバー変数

	// Transform
	Transform transform_;

	// ファイルパス
	std::string texturePath_;

	// バッファリソース
	ComPtr<ID3D12Resource> vertexResource_;
	ComPtr<ID3D12Resource> indexResource_;
	ComPtr<ID3D12Resource> materialResource_;
	ComPtr<ID3D12Resource> transformationMatrixResource_;

	// バッファリソース内のデータを参照するためのポインタ
	VertexData* vertexData_;
	uint32_t* indexData_;
	Material* materialData_;
	TransformationMatrix* transformationMatrixData_;

	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	// インデックスバッファビュー
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

	// テクスチャ番号
	uint32_t textureIndex_ = 0;

	// 座標
	Vector2 pos_ = { 0.0f, 0.0f };

	// 回転
	float rotation_ = 0.0f;

	// サイズ
	Vector2 size_ = { 640.0f, 360.0f };

	// アンカーポイント
	Vector2 anchorPoint_ = { 0.0f, 0.0f };

	// テクスチャの左上座標
	Vector2 texTopLeft_ = { 0.0f, 0.0f };

	// テクスチャの切り出しサイズ
	Vector2 texCutSize_ = { 0.0f, 0.0f };

	// 左右反転
	bool isFlipX_ = false;

	// 上下反転
	bool isFlipY_ = false;
};
