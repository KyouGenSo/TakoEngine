#pragma once

namespace Tako {

/// <summary>
/// トランジション効果のインターフェース
/// 全てのトランジション演出はこのインターフェースを実装する
/// </summary>
class ITransitionEffect
{
public:
	/// <summary>
	/// トランジションの状態
	/// </summary>
	enum TransitionState
	{
		NONE,      // 非アクティブ
		FADE_IN,   // フェードイン中（演出終了方向）
		FADE_OUT   // フェードアウト中（演出開始方向）
	};

public:
	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~ITransitionEffect() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	virtual void Initialize() = 0;

	/// <summary>
	/// 更新
	/// </summary>
	virtual void Update() = 0;

	/// <summary>
	/// 描画
	/// </summary>
	virtual void Draw() = 0;

	/// <summary>
	/// トランジション開始
	/// </summary>
	/// <param name="state">開始する状態（FADE_IN or FADE_OUT）</param>
	/// <param name="duration">演出時間（秒）</param>
	virtual void Start(TransitionState state, float duration) = 0;

	/// <summary>
	/// トランジション中止
	/// </summary>
	virtual void Stop() = 0;

	/// <summary>
	/// トランジションが終了したか
	/// </summary>
	/// <returns>終了していればtrue</returns>
	virtual bool IsFinished() const = 0;

	/// <summary>
	/// 現在の状態を取得
	/// </summary>
	/// <returns>現在のトランジション状態</returns>
	virtual TransitionState GetState() const = 0;
};

} // namespace Tako