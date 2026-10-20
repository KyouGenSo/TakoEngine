#pragma once
#include "EaseFunc.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include <json.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  /// <summary>
  /// トラックが動かす値。Translate は UI なら基準解像度 px・Object3d ならワールド単位、Rotate は度
  /// </summary>
  enum class AnimProperty : uint8_t {
    TranslateX, TranslateY, TranslateZ,
    RotateX, RotateY, RotateZ,
    ScaleX, ScaleY, ScaleZ,
    ColorR, ColorG, ColorB, ColorA,
    Frame,    ///< スプライトシートのコマ番号（UI のみ）
    Visible,  ///< 0.5 以上で表示
    Count
  };

  /// <summary>
  /// AnimProperty の保存名と静止時の値
  /// </summary>
  struct AnimPropertyInfo {
    const char* name;
    float       restValue;  ///< 上乗せしても見た目が変わらない値
  };

  /// <summary>
  /// 対象に上乗せする値。何も再生していなければ全て既定値（見た目は変わらない）
  /// </summary>
  struct AnimationPose {
    Vector3 translate = { 0.0f, 0.0f, 0.0f };        ///< 加算
    Vector3 rotate    = { 0.0f, 0.0f, 0.0f };        ///< 加算。度
    Vector3 scale     = { 1.0f, 1.0f, 1.0f };        ///< 乗算。UI は pivot が中心
    Vector4 color     = { 1.0f, 1.0f, 1.0f, 1.0f };  ///< 乗算
    int     frame     = -1;                          ///< 切り出し範囲を columns×rows に分けたコマ番号（左上から行順）。-1 なら切り替えない
    int     columns   = 1;
    int     rows      = 1;
    bool    visible   = true;
  };

  /// <summary>
  /// トラック上の 1 点
  /// </summary>
  struct AnimKey {
    float    time     = 0.0f;              ///< 秒
    float    value    = 0.0f;
    EaseType ease     = EaseType::Linear;  ///< このキーから次のキーへの補間
    Vector2  bezierP1 = { 0.42f, 0.0f };   ///< CubicBezier の制御点1（x:時間の割合 0～1, y:進行度）
    Vector2  bezierP2 = { 0.58f, 1.0f };
    uint32_t id       = 0;                 ///< AnimationClip が振る識別子。0 は未割り当て。ファイルには書かない
  };

  /// <summary>
  /// 1 つの対象の 1 つの値を動かすキーの列
  /// </summary>
  struct AnimTrack {
    std::string          target;                               ///< 要素名。空なら再生時に指定した対象
    AnimProperty         property = AnimProperty::TranslateX;
    int                  columns  = 1;                         ///< Frame のときだけ使うコマの列数
    int                  rows     = 1;
    std::vector<AnimKey> keys;                                 ///< 時刻順
  };

  /// <summary>
  /// 再生がこの時刻を過ぎたとき、PlayOptions::onEvent に name を渡す
  /// </summary>
  struct AnimEvent {
    float       time = 0.0f;
    std::string name;
    uint32_t    id   = 0;     ///< AnimationClip が振る識別子。ファイルには書かない
  };

  const AnimPropertyInfo& GetAnimPropertyInfo(AnimProperty property);

  /// <summary>
  /// GetAnimPropertyInfo の name の逆。該当なしなら空
  /// </summary>
  std::optional<AnimProperty> AnimPropertyFromString(std::string_view name);

  /// <summary>
  /// 名前付きのキーフレームアニメーション。kDirectory/name.json に保存し、同じ名前は 1 つのインスタンスを共有する
  /// </summary>
  class AnimationClip {
  public: //定数
    static constexpr const char* kDirectory = "resources/Json/Animations/";

  public: //構造体
    enum class LoopMode : uint8_t {
      Once,      ///< 最後の姿勢で止まる
      Loop,
      PingPong,  ///< 往復する（1 往復で duration の 2 倍）
      Count
    };

  public: //メンバー関数
    explicit AnimationClip(const std::string& name);

    // 共有インスタンスをポインタで配るので、コピーさせない
    AnimationClip(const AnimationClip&) = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;

    /// <summary>
    /// 名前のクリップを返す。初回だけ kDirectory から読み込み、ファイルが無ければ nullptr
    /// </summary>
    static AnimationClip* Find(const std::string& name);

    /// <summary>
    /// 空のクリップを登録して返す。同名が読み込み済みなら中身を空にする。ファイルは Save で作られる
    /// </summary>
    static AnimationClip* Create(const std::string& name);

    /// <summary>
    /// kDirectory/name.json で中身を置き換える（未保存の変更は捨てる）
    /// </summary>
    /// <returns>読めなければ false で中身は変えない</returns>
    bool Load();

    /// <summary>
    /// kDirectory/name.json へ保存する（ToJson から id を除いたもの）
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool Save() const;

    /// <summary>
    /// 保存する内容。includeIds ならキーとイベントの id も含める（Undo 用）
    /// </summary>
    [[nodiscard]] nlohmann::json ToJson(bool includeIds = true) const;

    /// <summary>
    /// ToJson の内容で置き換える。欠けている項目は既定値。id の無いキーとイベントには新しい id を振る
    /// </summary>
    void FromJson(const nlohmann::json& json);

    /// <summary>
    /// time の姿勢のうち、対象が name のトラックの値を pose に重ねる
    /// </summary>
    /// <param name="time">クリップ内の時刻（秒）</param>
    /// <param name="name">評価する要素名</param>
    /// <param name="boundTarget">target が空のトラックの対象</param>
    /// <param name="pose">加算・乗算で重ねる先</param>
    void EvaluateInto(float time, const std::string& name, const std::string& boundTarget, AnimationPose& pose) const;

    /// <summary>
    /// 再生開始からの経過秒を、ループの種類に従ってクリップ内の時刻にする
    /// </summary>
    [[nodiscard]] float WrapTime(float elapsed) const;

    /// <summary>
    /// 経過秒が (from, to] の間に通ったイベントを、通った順に out へ足す
    /// </summary>
    void CollectEvents(float from, float to, std::vector<const AnimEvent*>& out) const;

    /// <summary>
    /// キーとイベントを時刻順に並べ直す。時刻を編集した後に呼ぶ（id は変わらない）
    /// </summary>
    void SortByTime();

    /// <summary>
    /// キーとイベントに振る新しい id（キーとイベントで同じ番号は使わない）
    /// </summary>
    [[nodiscard]] uint32_t IssueId() { return nextId_++; }

    /// <summary>
    /// id のキー。trackIndex には見つかったトラックの添字を書く
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    [[nodiscard]] AnimKey* FindKey(uint32_t id, size_t* trackIndex = nullptr);

    /// <summary>
    /// id のイベント
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    [[nodiscard]] AnimEvent* FindEvent(uint32_t id);

    /// <summary>
    /// time のトラックの値。キーより前は最初の値、後は最後の値。Frame と Visible は補間しない
    /// </summary>
    [[nodiscard]] static float EvaluateTrack(const AnimTrack& track, float time);

    //============================================================
    //Setter
    //============================================================
    /// <summary>
    /// 長さ（秒）。負の値は 0 にする
    /// </summary>
    void SetDuration(float duration);

    void SetLoopMode(LoopMode mode) { loopMode_ = mode; }

    //============================================================
    //Getter
    //============================================================
    [[nodiscard]] const std::string& GetName() const { return name_; }
    [[nodiscard]] float GetDuration() const { return duration_; }
    [[nodiscard]] LoopMode GetLoopMode() const { return loopMode_; }
    [[nodiscard]] std::vector<AnimTrack>& GetTracks() { return tracks_; }
    [[nodiscard]] const std::vector<AnimTrack>& GetTracks() const { return tracks_; }
    [[nodiscard]] std::vector<AnimEvent>& GetEvents() { return events_; }
    [[nodiscard]] const std::vector<AnimEvent>& GetEvents() const { return events_; }

    /// <summary>
    /// ループの種類の保存名（"Once" など）
    /// </summary>
    [[nodiscard]] static const char* GetLoopModeName(LoopMode mode);

  private: //メンバー変数
    std::string            name_;
    float                  duration_ = 1.0f;            ///< 秒
    LoopMode               loopMode_ = LoopMode::Once;
    std::vector<AnimTrack> tracks_;
    std::vector<AnimEvent> events_;                     ///< 時刻順
    uint32_t               nextId_   = 1;
  };

} // namespace Tako
