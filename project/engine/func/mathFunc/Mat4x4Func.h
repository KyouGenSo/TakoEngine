#pragma once
#define _USE_MATH_DEFINES

#include "Matrix4x4.h"
#include "Vector3.h"
#include "Quaternion.h"

namespace Tako {

const int kRowHeight = 20;   ///< ImGuiデバッグ表示用の行の高さ
const int kColumnWidth = 60;  ///< ImGuiデバッグ表示用の列の幅

/// <summary>
/// 4x4行列演算ユーティリティ名前空間
/// アフィン変換（移動・回転・拡大縮小）、ビュー行列、プロジェクション行列など
/// 3Dグラフィックスに必要な全ての行列操作を提供
/// OpenGLスタイル（列優先）ではなくDirectXスタイル（行優先）で実装
/// </summary>
namespace Mat4x4 {
	/// <summary>行列の加算</summary>
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>行列の減算</summary>
	Matrix4x4 Subtrsct(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>行列の乗算（m1 * m2）</summary>
	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	/// <summary>逆行列を計算</summary>
	Matrix4x4 Inverse(const Matrix4x4& _m);

	/// <summary>転置行列を計算</summary>
	Matrix4x4 Transpose(const Matrix4x4& m);

	/// <summary>単位行列を生成</summary>
	Matrix4x4 MakeIdentity();

	/// <summary>平行移動行列を生成</summary>
	Matrix4x4 MakeTranslate(const Vector3& translate);

	/// <summary>拡大縮小行列を生成</summary>
	Matrix4x4 MakeScale(const Vector3& scale);

	/// <summary>X軸周りの回転行列を生成</summary>
	Matrix4x4 MakeRotateX(float angle);

	/// <summary>Y軸周りの回転行列を生成</summary>
	Matrix4x4 MakeRotateY(float angle);

	/// <summary>Z軸周りの回転行列を生成</summary>
	Matrix4x4 MakeRotateZ(float angle);

	/// <summary>X-Y-Z順のオイラー角回転行列を生成</summary>
	Matrix4x4 MakeRotateXYZ(Matrix4x4 mX, Matrix4x4 mY, Matrix4x4 mZ);
	/// <summary>オイラー角から回転行列を生成</summary>
	Matrix4x4 MakeRotateXYZ(const Vector3& rotate);
	/// <summary>クォータニオンから回転行列を生成</summary>
	Matrix4x4 MakeRotateXYZ(const Quaternion& rotate);

	/// <summary>アフィン変換行列を生成（オイラー角版）</summary>
	Matrix4x4 MakeAffine(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	/// <summary>アフィン変換行列を生成（クォータニオン版）</summary>
	Matrix4x4 MakeAffine(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	/// <summary>ベクトルを行列で変換（同次座標として扱う）</summary>
	Vector3 Transform(const Matrix4x4& m, const Vector3& v);

	/// <summary>法線ベクトルを行列で変換（平行移動を無視）</summary>
	Vector3 TransformNormal(const Matrix4x4& m, const Vector3& v);

	/// <summary>透視投影行列を生成</summary>
	Matrix4x4 MakePerspective(float fovY, float aspectRatio, float nearClip, float farClip);

	/// <summary>正射影行列を生成</summary>
	Matrix4x4 MakeOrtho(float left, float top, float right, float bottom, float nearClip, float farClip);

	/// <summary>ビューポート変換行列を生成</summary>
	Matrix4x4 MakeViewport(float left, float top, float width, float height, float minDepth, float maxDepth);

	/// <summary>逆転置行列を計算（法線変換用）</summary>
	Matrix4x4 InverseTranspose(const Matrix4x4& m);

	/// <summary>任意軸周りの回転行列を生成</summary>
	Matrix4x4 MakeRotateAxisAngle(const Vector3& axis, float angle);

	/// <summary>ある方向ベクトルから別の方向ベクトルへの回転行列を生成</summary>
	Matrix4x4 DirectionToDirection(const Vector3& from, const Vector3& to);

	/// <summary>
	/// 行列から位置、回転、スケールを分解
	/// </summary>
	/// <param name="matrix">分解する行列</param>
	/// <param name="position">出力: 位置</param>
	/// <param name="rotation">出力: 回転（オイラー角）</param>
	/// <param name="scale">出力: スケール</param>
	void Decompose(const Matrix4x4& matrix, Vector3& position, Vector3& rotation, Vector3& scale);

	/// <summary>
	/// 行列から回転成分を抽出（オイラー角として）
	/// </summary>
	/// <param name="matrix">回転を抽出する行列</param>
	/// <returns>回転（オイラー角）</returns>
	Vector3 ExtractEulerAngles(const Matrix4x4& matrix);
}

} // namespace Tako




