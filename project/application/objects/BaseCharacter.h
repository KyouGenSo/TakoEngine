#pragma once
#include "Object3d.h"
#include "Transform.h"
#include "memory"
#include "vector"
#include "cassert"
#include "collider.h"

class BaseCharacter : public Collider{
protected:
	std::vector<Object3d*> models_;
	Transform transform_;

public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="models">モデル</param>
	virtual void Initialize(const std::vector<Object3d*> models);

	/// <summary>
	/// 更新処理
	/// </summary>
	virtual void Update();

	/// <summary>
	/// 描画
	/// </summary>
	/// <param name="viewProjection">ビュープロジェクション</param>
	virtual void Draw();

	/// <summary>
	/// Getter
	/// </summary>
	const Transform& GetTransform() const { return transform_; }

	virtual Vector3 GetCenter() const override;
};
