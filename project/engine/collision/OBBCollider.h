#pragma once
#include "Collider.h"
#include "OBB.h"
#include "Vector3.h"
#include "Matrix4x4.h"

class OBBCollider : public Collider {
protected:
	Vector3 size_;           // ボックスのサイズ（幅、高さ、奥行き）
	Vector3 offset_;         // ローカルオフセット
	Matrix4x4 orientation_;  // ローカル回転行列
	
public:
	OBBCollider();
	virtual ~OBBCollider() = default;
	
	// OBBを取得（ワールド座標系）
	OBB GetOBB() const;
	
	// Colliderの仮想関数実装
	Vector3 GetCenter() const override;
	
	// サイズ設定
	void SetSize(const Vector3& size) { size_ = size; }
	Vector3 GetSize() const { return size_; }
	
	// オフセット設定
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	Vector3 GetOffset() const { return offset_; }
	
	// 回転設定
	void SetOrientation(const Matrix4x4& orientation) { orientation_ = orientation; }
	Matrix4x4 GetOrientation() const { return orientation_; }
};