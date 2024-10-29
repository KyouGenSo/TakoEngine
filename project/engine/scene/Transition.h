#pragma once


class Transition
{
private: // シングルトン設定
    // インスタンス
    static Transition* instance_;

    Transition() = default;
    ~Transition() = default;
    Transition(Transition&) = delete;
    Transition& operator=(Transition&) = delete;

public: // 列挙型
    enum TransitionType
    {
        FADE,
        SLIDE,
    };

    enum TransitionState
    {
        NONE,
        FADE_IN,
        FADE_OUT,
    };

public: // メンバ関数

    ///<summary>
    ///インスタンスの取得
    ///	</summary>
    static Transition* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新
    /// </summary>
    void Update();

    /// <summary>
    /// シーン遷移アニメション開始
    /// </summary>
    void Start(TransitionState state, TransitionType type, float duration);

    /// <summary>
    /// シーン遷移アニメション中止
    /// </summary>
    void Stop();

    /// <summary>
    /// シーン遷移アニメションが終了しているか
    /// </summary>
    bool IsFinished();

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

private: // メンバ変数

    TransitionType type_;

    TransitionState state_;

    float duration_;

    float transitionTime_;

    float transitionSpeed_;

    float alpha_;
};