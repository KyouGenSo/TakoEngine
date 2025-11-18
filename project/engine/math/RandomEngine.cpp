#include "RandomEngine.h"
#include <chrono>
#include <cmath>

// スレッドローカル変数の定義
thread_local std::mt19937 RandomEngine::generator_;
thread_local bool RandomEngine::initialized_ = false;

RandomEngine::RandomEngine() {
	// コンストラクタでは特に何もしない（初期化は遅延実行）
}

RandomEngine* RandomEngine::GetInstance() {
	static RandomEngine instance;
	// スレッドローカルなジェネレータが初期化されていない場合は初期化
	if (!initialized_) {
		instance.InitializeGenerator();
	}
	return &instance;
}

void RandomEngine::InitializeGenerator() {
	// ランダムデバイスからシードを取得
	std::random_device rd;
	generator_ = std::mt19937(rd());
	initialized_ = true;
}

float RandomEngine::GetFloat(float min, float max) {
	std::uniform_real_distribution<float> dist(min, max);
	return dist(generator_);
}

int RandomEngine::GetInt(int min, int max) {
	std::uniform_int_distribution<int> dist(min, max);
	return dist(generator_);
}

float RandomEngine::GetNormalized() {
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	return dist(generator_);
}

float RandomEngine::GetAngle() {
	return GetFloat(0.0f, k2Pi);
}

float RandomEngine::GetAngleRadians(float min, float max) {
	return GetFloat(min, max);
}

Vector3 RandomEngine::GetRandomDirection3D() {
	// 球面上の一様分布を生成
	// アルゴリズム: Marsaglia法を使用
	float theta = GetAngle();  // 方位角
	float phi = std::acos(1.0f - 2.0f * GetNormalized());  // 仰角

	float sinPhi = std::sin(phi);
	return Vector3(
		sinPhi * std::cos(theta),
		sinPhi * std::sin(theta),
		std::cos(phi)
	);
}

Vector2 RandomEngine::GetRandomDirection2D() {
	float angle = GetAngle();
	return Vector2(std::cos(angle), std::sin(angle));
}

Vector3 RandomEngine::GetRandomDirectionXZ() {
	float angle = GetAngle();
	return Vector3(std::cos(angle), 0.0f, std::sin(angle));
}

Vector3 RandomEngine::GetRandomPointInSphere(float radius) {
	// 拒絶サンプリング法を使用
	// より効率的な方法もあるが、実装がシンプルで理解しやすい
	float x, y, z;
	float lengthSq;

	do {
		x = GetFloat(-1.0f, 1.0f);
		y = GetFloat(-1.0f, 1.0f);
		z = GetFloat(-1.0f, 1.0f);
		lengthSq = x * x + y * y + z * z;
	} while (lengthSq > 1.0f);

	float scale = radius / std::sqrt(lengthSq);
	return Vector3(x * scale, y * scale, z * scale);
}

Vector2 RandomEngine::GetRandomPointInCircle(float radius) {
	// 極座標を使った一様分布
	float angle = GetAngle();
	// sqrt(r)を使って面積に対して一様にする
	float r = radius * std::sqrt(GetNormalized());
	return Vector2(r * std::cos(angle), r * std::sin(angle));
}

bool RandomEngine::GetBool(float probability) {
	// probabilityをクランプ
	if (probability <= 0.0f) return false;
	if (probability >= 1.0f) return true;

	return GetNormalized() < probability;
}

void RandomEngine::SetSeed(unsigned int seed) {
	generator_ = std::mt19937(seed);
	initialized_ = true;
}

void RandomEngine::SetRandomSeed() {
	// 現在時刻をシードとして使用
	auto now = std::chrono::high_resolution_clock::now();
	auto seed = static_cast<unsigned int>(now.time_since_epoch().count());
	SetSeed(seed);
}