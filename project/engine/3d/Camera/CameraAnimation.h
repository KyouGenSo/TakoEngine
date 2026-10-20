#pragma once
#include "CameraKeyframe.h"
#include "Camera.h"
#include "Quaternion.h"
#include "Transform.h"
#include <json.hpp>
#include <vector>
#include <string>

namespace Tako {

  /// <summary>
  /// キーフレーム間補間でカメラを動かすアニメーション
  /// </summary>
  class CameraAnimation {
  public: //定数
    static constexpr const char* kDirectory = "resources/Json/CameraAnimations/";

  private: //定数
    static constexpr int   kArcSamplesPerSegment = 32;    ///< 経路の長さを測るとき1区間を何分割するか
    static constexpr float kDefaultBlendDuration = 0.5f;  ///< 秒

  public: //構造体
    enum class PlayState {
      STOPPED,
      PLAYING,
      PAUSED
    };

    enum class StartMode {
      JUMP_CUT,        ///< 即座に最初のキーフレームへ
      SMOOTH_BLEND     ///< 現在位置から最初のキーフレームまで補間
    };

    enum class TimingMode {
      PER_SEGMENT,     ///< キーフレーム区間ごとにイージングを掛ける（各キーフレームで減速・停止しやすい）
      UNIFIED_WARP,    ///< 各キーフレームの到達時刻は守り、イージングはアニメ全体に1本だけ掛ける（区間の境目で止まらない）
      CONSTANT_SPEED   ///< 経路上を常に一定速度で移動する。中間キーフレームの時刻は無視され、位置は経路の形にだけ使われる
    };

    /// <summary>
    /// ある時刻にカメラが取るべき状態
    /// </summary>
    struct Pose {
      Vector3 position = { 0.0f, 0.0f, 0.0f };  ///< ワールド座標（TARGET_RELATIVE はターゲット位置を加算済み）
      Vector3 rotation = { 0.0f, 0.0f, 0.0f };  ///< オイラー角、ラジアン
      float   fov      = 0.45f;                 ///< ラジアン
    };

  public: //メンバー関数
    CameraAnimation();
    ~CameraAnimation();

    // 生存インスタンス一覧に自分のアドレスを載せるので、コピーさせない
    CameraAnimation(const CameraAnimation&) = delete;
    CameraAnimation& operator=(const CameraAnimation&) = delete;

    void Update(float deltaTime);

    /// <summary>
    /// キーフレームを新しい id を振って追加。時刻順にソートし総時間を更新
    /// </summary>
    /// <param name="keyframe">追加するキーフレーム（id は無視する）</param>
    void AddKeyframe(const CameraKeyframe& keyframe);

    /// <summary>
    /// 現在のカメラ状態から指定時刻のキーフレームを生成して追加。camera_ 未設定時は何もしない
    /// </summary>
    /// <param name="time">キーフレーム時刻（秒）</param>
    /// <param name="interpolation">このキーフレームから次への補間方法</param>
    void AddKeyframeFromCurrentCamera(float time,
      CameraKeyframe::InterpolationType interpolation = CameraKeyframe::InterpolationType::LINEAR);

    /// <summary>
    /// 指定 index のキーフレームを削除し総時間を更新。範囲外は無視
    /// </summary>
    /// <param name="index">削除するキーフレームの添字</param>
    void RemoveKeyframe(size_t index);

    /// <summary>
    /// 指定 index のキーフレームを置換。id はそのまま残し、時刻順に再ソートして総時間を更新。範囲外は無視
    /// </summary>
    /// <param name="index">置換するキーフレームの添字</param>
    /// <param name="keyframe">新しい内容（id は無視する）</param>
    void EditKeyframe(size_t index, const CameraKeyframe& keyframe);

    /// <summary>
    /// id のキーフレームの添字（時刻順の並び替えで変わるので、持ち続けるなら id で持つ）
    /// </summary>
    /// <returns>無ければ -1</returns>
    [[nodiscard]] int FindKeyframeIndex(uint32_t id) const;

    void ClearKeyframes();

    void Play();
    void Pause();

    /// <summary>
    /// 停止し時間を0に戻す
    /// </summary>
    void Stop();

    /// <summary>
    /// FOV 復元なしで停止（アニメーション切り替え時用）
    /// </summary>
    void StopWithoutRestore();

    /// <summary>
    /// 現在時刻のみ0に戻す（状態はそのまま）
    /// </summary>
    void Reset();

    /// <summary>
    /// kDirectory の JSON を読み込み、アニメーション名をファイル名にする
    /// </summary>
    /// <param name="filepath">kDirectory からの相対パス（拡張子省略時は .json）</param>
    /// <returns>読み込めたら true</returns>
    bool LoadFromJson(const std::string& filepath);

    /// <summary>
    /// kDirectory へ JSON で保存する（ToJson からキーフレームの id を除いたもの）
    /// </summary>
    /// <param name="filepath">kDirectory からの相対パス（拡張子省略時は .json）</param>
    /// <returns>書き込めたら true</returns>
    bool SaveToJson(const std::string& filepath) const;

    /// <summary>
    /// 保存する内容（キーフレームと再生設定）とキーフレームの id。名前・カメラ・ターゲット・再生状態は含まない
    /// </summary>
    [[nodiscard]] nlohmann::json ToJson() const;

    /// <summary>
    /// ToJson の内容で置き換える。欠けている項目は既定値（旧フォーマット互換）。id の無いキーフレームには新しい id を振る
    /// </summary>
    void FromJson(const nlohmann::json& json);

    /// <summary>
    /// 指定時刻の姿勢を求める。カメラにも再生状態にも触れない
    /// </summary>
    /// <param name="time">対象時刻（秒）</param>
    /// <returns>姿勢。キーフレームが1個ならその姿勢、0個なら Pose の既定値</returns>
    [[nodiscard]] Pose EvaluatePoseAtTime(float time) const;

    /// <summary>
    /// 補間係数 t にキーフレームのイージングを適用（CUBIC_BEZIER は制御点で評価）
    /// </summary>
    /// <param name="t">入力係数（0.0～1.0）</param>
    /// <param name="kf">補間方法と制御点の参照元キーフレーム</param>
    /// <returns>イージング後の係数</returns>
    static float ApplyEasing(float t, const CameraKeyframe& kf);

    /// <summary>
    /// 補間係数 t にイージングを適用（CUBIC_BEZIER は p1/p2 の制御点で評価）
    /// </summary>
    /// <param name="t">入力係数（0.0～1.0）</param>
    /// <param name="type">補間方法</param>
    /// <param name="p1">CUBIC_BEZIER 制御点1</param>
    /// <param name="p2">CUBIC_BEZIER 制御点2</param>
    /// <returns>イージング後の係数</returns>
    static float ApplyEasing(float t, CameraKeyframe::InterpolationType type, const Vector2& p1, const Vector2& p2);

#ifdef _DEBUG
    /// <summary>
    /// 生存中のアニメーション一覧（エディタ用。生成順）
    /// </summary>
    /// <returns>生存中の全インスタンス</returns>
    static const std::vector<CameraAnimation*>& GetInstances();
#endif

    //==========================================================================
    //Setter
    //==========================================================================
    void SetCamera(Camera* camera) { camera_ = camera; }

    /// <summary>
    /// 相対座標の基準ターゲット（nullptr で解除）
    /// </summary>
    void SetTarget(const Transform* target) { targetTransform_ = target; }

    void SetLooping(bool loop) { isLooping_ = loop; }
    void SetPlaySpeed(float speed) { playSpeed_ = speed; }
    void SetAnimationName(const std::string& name) { animationName_ = name; }

    /// <summary>
    /// 現在時刻を設定（シーク）
    /// </summary>
    void SetCurrentTime(float time);

    void SetStartMode(StartMode mode) { startMode_ = mode; }
    void SetBlendDuration(float duration) { blendDuration_ = duration; }

    void SetTimingMode(TimingMode mode) { timingMode_ = mode; }
    void SetGlobalEasing(CameraKeyframe::InterpolationType type) { globalEasing_ = type; }

    /// <summary>
    /// 全体イージングのベジェ制御点1。x は [0,1] に clamp
    /// </summary>
    void SetGlobalBezierP1(const Vector2& p1);

    /// <summary>
    /// 全体イージングのベジェ制御点2。x は [0,1] に clamp
    /// </summary>
    void SetGlobalBezierP2(const Vector2& p2);

    //==========================================================================
    //Getter
    //==========================================================================
    [[nodiscard]] size_t GetKeyframeCount() const { return keyframes_.size(); }
    [[nodiscard]] const CameraKeyframe& GetKeyframe(size_t index) const { return keyframes_[index]; }
    [[nodiscard]] float GetDuration() const { return duration_; }
    [[nodiscard]] float GetPlaybackTime() const { return currentTime_; }
    [[nodiscard]] float GetPlaySpeed() const { return playSpeed_; }
    [[nodiscard]] PlayState GetPlayState() const { return playState_; }
    [[nodiscard]] bool IsLooping() const { return isLooping_; }
    [[nodiscard]] const std::string& GetAnimationName() const { return animationName_; }
    [[nodiscard]] const Transform* GetTarget() const { return targetTransform_; }
    [[nodiscard]] StartMode GetStartMode() const { return startMode_; }
    [[nodiscard]] float GetBlendDuration() const { return blendDuration_; }
    [[nodiscard]] TimingMode GetTimingMode() const { return timingMode_; }
    [[nodiscard]] CameraKeyframe::InterpolationType GetGlobalEasing() const { return globalEasing_; }
    [[nodiscard]] const Vector2& GetGlobalBezierP1() const { return globalBezierP1_; }
    [[nodiscard]] const Vector2& GetGlobalBezierP2() const { return globalBezierP2_; }
    [[nodiscard]] bool IsBlending() const { return isBlending_; }

    /// <summary>
    /// ブレンド進行度を取得（0.0～1.0）
    /// </summary>
    [[nodiscard]] float GetBlendProgress() const { return blendProgress_; }

  private: //非公開関数
    void SortKeyframes();
    void UpdateDuration();

    /// <summary>
    /// time を挟む前後キーフレームの index を求める
    /// </summary>
    /// <param name="time">対象時刻（秒）</param>
    /// <param name="prevIndex">出力。time 以下で最大時刻のキーフレーム添字</param>
    /// <param name="nextIndex">出力。prevIndex の次の添字。末尾超過時はループなら0、非ループなら prevIndex</param>
    /// <returns>キーフレームが2個以上あれば true、未満なら false</returns>
    bool FindKeyframeIndices(float time, size_t& prevIndex, size_t& nextIndex) const;

    /// <summary>
    /// 指定時刻の区間を求め、イージング適用済み係数で補間してカメラへ適用
    /// </summary>
    /// <param name="time">対象時刻（秒）</param>
    void ApplyAnimationAtTime(float time);

    /// <summary>
    /// prevIndex/nextIndex 区間を係数 t で補間した姿勢。座標系は prev 側を採用
    /// </summary>
    /// <param name="prevIndex">補間元キーフレームの添字</param>
    /// <param name="nextIndex">補間先キーフレームの添字</param>
    /// <param name="t">補間係数（0.0～1.0、イージング適用後）。位置は pathType に従い、FOVは線形、回転は Slerp</param>
    /// <returns>補間した姿勢</returns>
    Pose InterpolateKeyframes(size_t prevIndex, size_t nextIndex, float t) const;

    /// <summary>
    /// 1 つのキーフレームの姿勢（TARGET_RELATIVE はターゲット位置を加算）
    /// </summary>
    Pose KeyframePose(const CameraKeyframe& kf) const;

    /// <summary>
    /// 姿勢をカメラへ書き込む。camera_ 未設定時は何もしない
    /// </summary>
    void ApplyPose(const Pose& pose);

    /// <summary>
    /// 区間内の位置を pathType に従って計算する（LINEAR は直線、CATMULL_ROM は前後の点も使った曲線）。
    /// TARGET_RELATIVE のオフセット→ワールド変換はまだ行わない
    /// </summary>
    /// <param name="prevIndex">補間元キーフレームの添字</param>
    /// <param name="nextIndex">補間先キーフレームの添字</param>
    /// <param name="easedT">イージング適用済み補間係数（0.0～1.0）</param>
    /// <returns>セグメント空間の位置</returns>
    Vector3 EvaluateSegmentPosition(size_t prevIndex, size_t nextIndex, float easedT) const;

    /// <summary>
    /// 再生時刻から「どの区間を・どこまで進んだか」を timingMode_ に従って求める（出力 t はイージング適用済み）
    /// </summary>
    /// <param name="time">対象時刻（秒）</param>
    /// <param name="prevIndex">出力。補間元キーフレームの添字</param>
    /// <param name="nextIndex">出力。補間先キーフレームの添字</param>
    /// <param name="t">出力。補間係数（0.0～1.0）</param>
    /// <returns>キーフレームが2個以上あれば true</returns>
    bool ResolveSegmentAtTime(float time, size_t& prevIndex, size_t& nextIndex, float& t) const;

    /// <summary>
    /// CONSTANT_SPEED 用の累積距離テーブルを、キーフレーム変更後の初回アクセス時だけ作り直す
    /// </summary>
    void EnsureArcLengthTable() const;

    /// <summary>
    /// オイラー角からクォータニオンへ変換（回転順序 Y*X*Z）
    /// </summary>
    /// <param name="euler">オイラー角（ラジアン）</param>
    /// <returns>対応するクォータニオン</returns>
    Quaternion EulerToQuaternion(const Vector3& euler) const;

    /// <summary>
    /// クォータニオンからオイラー角へ変換
    /// </summary>
    /// <param name="q">入力クォータニオン</param>
    /// <returns>オイラー角（ラジアン）。ジンバルロック時は y を±π/2に固定</returns>
    Vector3 QuaternionToEuler(const Quaternion& q) const;

  private: //メンバー変数
    //基本情報
    std::string                 animationName_   = "Untitled";
    std::vector<CameraKeyframe> keyframes_;
    uint32_t                    nextKeyframeId_  = 1;           ///< 次に振るキーフレームの id
    Camera*                     camera_          = nullptr;
    const Transform*            targetTransform_ = nullptr;     ///< 相対座標の基準

    //再生状態
    float     currentTime_ = 0.0f;                ///< 秒
    float     duration_    = 0.0f;                ///< 秒
    float     playSpeed_   = 1.0f;                ///< 1.0 が標準
    PlayState playState_   = PlayState::STOPPED;
    bool      isLooping_   = false;

    //タイミングモード（UNIFIED_WARP/CONSTANT_SPEED では区間イージングの代わりに全体へ1本掛ける）
    TimingMode                        timingMode_     = TimingMode::PER_SEGMENT;
    CameraKeyframe::InterpolationType globalEasing_   = CameraKeyframe::InterpolationType::LINEAR;
    Vector2                           globalBezierP1_ = { 0.42f, 0.0f };
    Vector2                           globalBezierP2_ = { 0.58f, 1.0f };

    //弧長キャッシュ（CONSTANT_SPEED 用。経路を細かく刻んだ累積距離表。キーフレーム変更で作り直す）
    mutable std::vector<float> arcLengths_;             ///< 経路の始点から各サンプル点までの累積距離
    mutable float              totalArcLength_ = 0.0f;
    mutable bool               arcLengthDirty_ = true;

    //ブレンド
    StartMode startMode_     = StartMode::JUMP_CUT;
    float     blendDuration_ = kDefaultBlendDuration;  ///< 秒
    float     blendProgress_ = 0.0f;                   ///< 0.0～1.0
    bool      isBlending_    = false;

    //ブレンド開始時のカメラ状態
    Vector3 blendStartPosition_;
    Vector3 blendStartRotation_;
    float   blendStartFov_      = 0.0f;

    //FOV 復元
    float originalFov_    = 0.0f;   ///< アニメーション開始前の FOV
    bool  hasOriginalFov_ = false;
  };

} // namespace Tako
