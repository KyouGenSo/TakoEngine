#pragma once
#include <random>
#include <thread>
#include "Vector2.h"
#include "Vector3.h"

namespace Tako {

/// <summary>
/// 乱数生成エンジン
/// シングルトンパターンで実装され、アプリケーション全体で統一された乱数生成を提供
/// </summary>
class RandomEngine final {
public:
	/// <summary>
	/// インスタンスの取得
	/// </summary>
	/// <returns>RandomEngineのシングルトンインスタンス</returns>
	static RandomEngine* GetInstance();

	// 削除されたコンストラクタと代入演算子
	RandomEngine(const RandomEngine&) = delete;
	RandomEngine& operator=(const RandomEngine&) = delete;

	/// <summary>
	/// 指定範囲の浮動小数点数を生成
	/// </summary>
	/// <param name="min">最小値（含む）</param>
	/// <param name="max">最大値（含む）</param>
	/// <returns>生成された乱数</returns>
	float GetFloat(float min, float max);

	/// <summary>
	/// 指定範囲の整数を生成
	/// </summary>
	/// <param name="min">最小値（含む）</param>
	/// <param name="max">最大値（含む）</param>
	/// <returns>生成された乱数</returns>
	int GetInt(int min, int max);

	/// <summary>
	/// 0から1の範囲の浮動小数点数を生成
	/// </summary>
	/// <returns>0.0f～1.0fの乱数</returns>
	float GetNormalized();

	/// <summary>
	/// 0から2πの範囲の角度を生成（ラジアン）
	/// </summary>
	/// <returns>0～2πの角度（ラジアン）</returns>
	float GetAngle();

	/// <summary>
	/// 指定範囲の角度を生成（ラジアン）
	/// </summary>
	/// <param name="min">最小角度（ラジアン）</param>
	/// <param name="max">最大角度（ラジアン）</param>
	/// <returns>生成された角度（ラジアン）</returns>
	float GetAngleRadians(float min, float max);

	/// <summary>
	/// 正規化された3次元ランダム方向ベクトルを生成
	/// </summary>
	/// <returns>長さ1の3Dベクトル</returns>
	Vector3 GetRandomDirection3D();

	/// <summary>
	/// 正規化された2次元ランダム方向ベクトルを生成
	/// </summary>
	/// <returns>長さ1の2Dベクトル</returns>
	Vector2 GetRandomDirection2D();

	/// <summary>
	/// XZ平面上の正規化されたランダム方向ベクトルを生成（Y=0）
	/// </summary>
	/// <returns>Y=0の長さ1の3Dベクトル</returns>
	Vector3 GetRandomDirectionXZ();

	/// <summary>
	/// 球内のランダムな点を生成
	/// </summary>
	/// <param name="radius">球の半径</param>
	/// <returns>球内のランダムな3D座標</returns>
	Vector3 GetRandomPointInSphere(float radius);

	/// <summary>
	/// 円内のランダムな点を生成
	/// </summary>
	/// <param name="radius">円の半径</param>
	/// <returns>円内のランダムな2D座標</returns>
	Vector2 GetRandomPointInCircle(float radius);

	/// <summary>
	/// 確率判定（指定した確率でtrueを返す）
	/// </summary>
	/// <param name="probability">確率（0.0～1.0）</param>
	/// <returns>判定結果</returns>
	bool GetBool(float probability = 0.5f);

	/// <summary>
	/// シードの設定（デバッグ用）
	/// </summary>
	/// <param name="seed">シード値</param>
	void SetSeed(unsigned int seed);

	/// <summary>
	/// ランダムシードの設定（現在時刻ベース）
	/// </summary>
	void SetRandomSeed();

private:
	/// <summary>
	/// プライベートコンストラクタ
	/// </summary>
	RandomEngine();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~RandomEngine() = default;

	/// <summary>
	/// ジェネレータの初期化
	/// </summary>
	void InitializeGenerator();

	// スレッドローカルストレージでジェネレータを管理
	static thread_local std::mt19937 generator_;
	static thread_local bool initialized_;

	// π定数
	static constexpr float kPi = 3.14159265358979323846f;
	static constexpr float k2Pi = kPi * 2.0f;
};

} // namespace Tako