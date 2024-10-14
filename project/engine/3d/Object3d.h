#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"

class Model;

class Camera;

class Object3d {

public: // 構造体

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
	};

	// 平行光源データ
	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		int32_t lightType;
		float intensity;
	};

public: // メンバー関数
	///<summary>
	///初期化
	/// </summary>
	void Initialize();

	///<summary>
	///更新
	/// </summary>
	void Update();

	///<summary>
	///描画
	/// </summary>
	void Draw();

	//-----------------------------------------Getter-----------------------------------------//
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	
	//-----------------------------------------Setter-----------------------------------------//
	void SetModel(const std::string& fileName);
	void SetCamera(Camera* camera) { m_camera_ = camera; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

private: // プライベートメンバー関数

	///<summary>
	///座標変換行列データの生成
	/// </summary>
	void CreateTransformationMatrixData();

	///<summary>
	///平行光源データの生成
	/// </summary>
	void CreateDirectionalLightData();

private: // メンバー変数

	// カメラのクラスポインター
	Camera* m_camera_ = nullptr;

	// モデルクラス
	Model* m_model_ = nullptr;

	// トランスフォーム
	Transform transform_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;

};
