#include "CameraAnimation.h"
#include "Vec3Func.h"
#include "QuatFunc.h"
#include "EaseFunc.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <numbers>
#include <format>
#include <utility>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  namespace {

    /// <summary>
    /// kDirectory からの相対パスをファイルパスにする（拡張子省略時は .json）
    /// </summary>
    std::filesystem::path ToFilePath(const std::string& filepath) {
      std::filesystem::path path = std::filesystem::path(CameraAnimation::kDirectory) / filepath;
      if (!path.has_extension()) {
        path += ".json";
      }
      return path;
    }

#ifdef _DEBUG
    std::vector<CameraAnimation*>& Instances() {
      static std::vector<CameraAnimation*> instances;
      return instances;
    }
#endif

  } // namespace

  CameraAnimation::CameraAnimation() {
#ifdef _DEBUG
    Instances().push_back(this);
#endif
  }

  CameraAnimation::~CameraAnimation() {
#ifdef _DEBUG
    std::erase(Instances(), this);
#endif
  }

#ifdef _DEBUG
  const std::vector<CameraAnimation*>& CameraAnimation::GetInstances() {
    return Instances();
  }
#endif

  void CameraAnimation::Update(float deltaTime) {
    if (!camera_) {
      return;
    }

    if (playState_ != PlayState::PLAYING) {
      return;
    }

    if (isBlending_) {
      blendProgress_ += deltaTime / blendDuration_;

      if (blendProgress_ >= 1.0f) {
        blendProgress_ = 1.0f;
        isBlending_ = false;
      }

      // 現在のカメラ状態から最初のキーフレームまで補間
      if (!keyframes_.empty()) {
        const Pose target = KeyframePose(keyframes_[0]);
        float t = Ease::InOutQuad(blendProgress_);

        // 回転は Slerp
        Quaternion q1 = EulerToQuaternion(blendStartRotation_);
        Quaternion q2 = EulerToQuaternion(target.rotation);
        Quaternion qResult = Quat::Slerp(q1, q2, t);

        ApplyPose({
          .position = Vec3::Lerp(blendStartPosition_, target.position, t),
          .rotation = QuaternionToEuler(qResult),
          .fov      = Vec3::Lerp(blendStartFov_, target.fov, t),
        });
      }

      // ブレンド中は通常のアニメーション処理をスキップ
      return;
    }

    if (keyframes_.size() < 2) {
      return;
    }

    currentTime_ += deltaTime * playSpeed_;

    if (currentTime_ >= duration_) {
      if (isLooping_) {
        currentTime_ = fmodf(currentTime_, duration_);
      }
      else {
        currentTime_ = duration_;
        playState_ = PlayState::STOPPED;

        if (hasOriginalFov_) {
          camera_->SetFovY(originalFov_);
          hasOriginalFov_ = false;
        }
      }
    }

    // 逆再生で時間が負になった場合
    if (currentTime_ < 0.0f) {
      if (isLooping_) {
        currentTime_ = duration_ + fmodf(currentTime_, duration_);
      }
      else {
        currentTime_ = 0.0f;
        playState_ = PlayState::STOPPED;

        if (hasOriginalFov_) {
          camera_->SetFovY(originalFov_);
          hasOriginalFov_ = false;
        }
      }
    }

    ApplyAnimationAtTime(currentTime_);
  }

  void CameraAnimation::AddKeyframe(const CameraKeyframe& keyframe) {
    keyframes_.push_back(keyframe);
    keyframes_.back().id = nextKeyframeId_++;
    SortKeyframes();
    UpdateDuration();
  }

  void CameraAnimation::AddKeyframeFromCurrentCamera(float time,
    CameraKeyframe::InterpolationType interpolation) {

    if (!camera_) {
      return;
    }

    CameraKeyframe keyframe;
    keyframe.time = time;
    keyframe.position = camera_->GetTranslate();
    keyframe.rotation = camera_->GetRotate();
    keyframe.fov = camera_->GetFovY();
    keyframe.interpolation = interpolation;

    AddKeyframe(keyframe);
  }

  void CameraAnimation::RemoveKeyframe(size_t index) {
    if (index >= keyframes_.size()) {
      return;
    }

    keyframes_.erase(keyframes_.begin() + index);
    UpdateDuration();
  }

  void CameraAnimation::EditKeyframe(size_t index, const CameraKeyframe& keyframe) {
    if (index >= keyframes_.size()) {
      return;
    }

    const uint32_t id = keyframes_[index].id;
    keyframes_[index] = keyframe;
    keyframes_[index].id = id;
    SortKeyframes();
    UpdateDuration();
  }

  int CameraAnimation::FindKeyframeIndex(uint32_t id) const {
    const auto it = std::ranges::find(keyframes_, id, &CameraKeyframe::id);
    return it == keyframes_.end() ? -1 : static_cast<int>(it - keyframes_.begin());
  }

  void CameraAnimation::ClearKeyframes() {
    keyframes_.clear();
    duration_ = 0.0f;
    currentTime_ = 0.0f;
    playState_ = PlayState::STOPPED;
    arcLengthDirty_ = true;
  }

  void CameraAnimation::Play() {
    if (keyframes_.empty() || !camera_) {
      return;
    }

    playState_ = PlayState::PLAYING;

    originalFov_ = camera_->GetFovY();
    hasOriginalFov_ = true;

    if (startMode_ == StartMode::JUMP_CUT) {
      currentTime_ = 0.0f;
      isBlending_ = false;
      ApplyPose(KeyframePose(keyframes_[0]));
    }
    else {
      // SMOOTH_BLEND: 現在のカメラ状態を起点にブレンド開始
      blendStartPosition_ = camera_->GetTranslate();
      blendStartRotation_ = camera_->GetRotate();
      blendStartFov_ = camera_->GetFovY();
      blendProgress_ = 0.0f;
      isBlending_ = true;
      currentTime_ = 0.0f;
    }
  }

  void CameraAnimation::Pause() {
    if (playState_ == PlayState::PLAYING) {
      playState_ = PlayState::PAUSED;
    }
  }

  void CameraAnimation::Stop() {
    playState_ = PlayState::STOPPED;
    currentTime_ = 0.0f;

    if (hasOriginalFov_ && camera_) {
      camera_->SetFovY(originalFov_);
      hasOriginalFov_ = false;
    }
  }

  void CameraAnimation::StopWithoutRestore() {
    playState_ = PlayState::STOPPED;
    currentTime_ = 0.0f;

    // FOV は復元せずフラグのみリセット
    hasOriginalFov_ = false;
  }

  void CameraAnimation::Reset() {
    currentTime_ = 0.0f;
  }

  void CameraAnimation::SetCurrentTime(float time) {
    currentTime_ = std::clamp(time, 0.0f, duration_);

    // 再生状態に関係なくプレビュー/スクラブのため補間を実行
    if (!camera_ || keyframes_.size() < 2) {
      return;
    }

    ApplyAnimationAtTime(currentTime_);
  }

  void CameraAnimation::SetGlobalBezierP1(const Vector2& p1) {
    globalBezierP1_ = { std::clamp(p1.x, 0.0f, 1.0f), p1.y };
  }

  void CameraAnimation::SetGlobalBezierP2(const Vector2& p2) {
    globalBezierP2_ = { std::clamp(p2.x, 0.0f, 1.0f), p2.y };
  }

  void CameraAnimation::SortKeyframes() {
    // 同じ時刻のキーフレームの順番を保つ（Undo の復元で入れ替わると添字の選択がずれる）
    std::ranges::stable_sort(keyframes_, {}, &CameraKeyframe::time);
  }

  void CameraAnimation::UpdateDuration() {
    // キーフレームが変わると必ずここを通るので、弧長テーブルの作り直しフラグもここで立てる
    arcLengthDirty_ = true;

    if (keyframes_.empty()) {
      duration_ = 0.0f;
      return;
    }

    // 末尾キーフレームの時刻が総時間
    duration_ = keyframes_.back().time;
  }

  bool CameraAnimation::FindKeyframeIndices(float time, size_t& prevIndex, size_t& nextIndex) const {
    if (keyframes_.size() < 2) {
      return false;
    }

    // time 以下で最大の時刻を持つキーフレーム
    prevIndex = 0;
    for (size_t i = 0; i < keyframes_.size(); ++i) {
      if (keyframes_[i].time <= time) {
        prevIndex = i;
      }
      else {
        break;
      }
    }

    nextIndex = prevIndex + 1;
    if (nextIndex >= keyframes_.size()) {
      // 末尾を超えた場合、ループ時は先頭へ戻し、非ループ時は末尾を維持
      if (isLooping_ && keyframes_.size() > 1) {
        nextIndex = 0;
      }
      else {
        nextIndex = prevIndex;
      }
    }

    return true;
  }

  void CameraAnimation::ApplyAnimationAtTime(float time) {
    size_t prevIndex = 0, nextIndex = 0;
    float t = 0.0f;
    if (!ResolveSegmentAtTime(time, prevIndex, nextIndex, t)) {
      return;
    }

    ApplyPose(InterpolateKeyframes(prevIndex, nextIndex, t));
  }

  bool CameraAnimation::ResolveSegmentAtTime(float time, size_t& prevIndex, size_t& nextIndex, float& t) const {
    if (keyframes_.size() < 2) {
      return false;
    }

    switch (timingMode_) {
    case TimingMode::UNIFIED_WARP:
    case TimingMode::CONSTANT_SPEED: {
      float progress = (duration_ > 0.0f) ? std::clamp(time / duration_, 0.0f, 1.0f) : 0.0f;
      progress = ApplyEasing(progress, globalEasing_, globalBezierP1_, globalBezierP2_);

      if (timingMode_ == TimingMode::CONSTANT_SPEED) {
        EnsureArcLengthTable();
        if (totalArcLength_ > 0.0f) {
          // 「スタートから何 m 進んだ地点か」を先に決め、累積距離表からその地点が属する区間と区間内の進み具合を逆引きする
          float targetLength = progress * totalArcLength_;
          auto it = std::lower_bound(arcLengths_.begin(), arcLengths_.end(), targetLength);
          size_t k = static_cast<size_t>(it - arcLengths_.begin());
          if (k >= arcLengths_.size()) {
            k = arcLengths_.size() - 1;
          }

          float prevLength = (k > 0) ? arcLengths_[k - 1] : 0.0f;
          float spanLength = arcLengths_[k] - prevLength;
          float frac = (spanLength > 0.0f) ? (targetLength - prevLength) / spanLength : 0.0f;

          prevIndex = k / kArcSamplesPerSegment;
          nextIndex = prevIndex + 1;
          t = (static_cast<float>(k % kArcSamplesPerSegment) + frac) / static_cast<float>(kArcSamplesPerSegment);
          t = std::clamp(t, 0.0f, 1.0f);
          return true;
        }
        // 経路の長さが 0（全キーフレームが同位置）だと距離で割れないため、時刻ベースの補間で代用
      }

      // イージングはアニメ全体の進行度にだけ掛けて時刻を変換する。
      // 区間ごとには掛けないので、キーフレームの境目で速度が 0 にならない
      float warpedTime = progress * duration_;
      if (!FindKeyframeIndices(warpedTime, prevIndex, nextIndex)) {
        return false;
      }
      const CameraKeyframe& prev = keyframes_[prevIndex];
      const CameraKeyframe& next = keyframes_[nextIndex];
      float timeDiff = next.time - prev.time;
      t = (timeDiff > 0.0f) ? std::clamp((warpedTime - prev.time) / timeDiff, 0.0f, 1.0f) : 0.0f;
      return true;
    }

    case TimingMode::PER_SEGMENT:
    default: {
      if (!FindKeyframeIndices(time, prevIndex, nextIndex)) {
        return false;
      }
      const CameraKeyframe& prev = keyframes_[prevIndex];
      const CameraKeyframe& next = keyframes_[nextIndex];
      float timeDiff = next.time - prev.time;
      t = 0.0f;
      if (timeDiff > 0.0f) {
        t = std::clamp((time - prev.time) / timeDiff, 0.0f, 1.0f);
        t = ApplyEasing(t, prev);
      }
      return true;
    }
    }
  }

  void CameraAnimation::EnsureArcLengthTable() const {
    if (!arcLengthDirty_) {
      return;
    }
    arcLengthDirty_ = false;
    arcLengths_.clear();
    totalArcLength_ = 0.0f;

    if (keyframes_.size() < 2) {
      return;
    }

    // 各区間を細かい直線に分割し、長さを足し込んで累積距離表を作る
    arcLengths_.reserve((keyframes_.size() - 1) * kArcSamplesPerSegment);
    float accumulated = 0.0f;
    for (size_t i = 0; i + 1 < keyframes_.size(); ++i) {
      Vector3 prevPos = EvaluateSegmentPosition(i, i + 1, 0.0f);
      for (int j = 1; j <= kArcSamplesPerSegment; ++j) {
        float sampleT = static_cast<float>(j) / kArcSamplesPerSegment;
        Vector3 pos = EvaluateSegmentPosition(i, i + 1, sampleT);
        accumulated += static_cast<float>(Vec3::Length(Vec3::Subtract(pos, prevPos)));
        arcLengths_.push_back(accumulated);
        prevPos = pos;
      }
    }
    totalArcLength_ = accumulated;
  }

  CameraAnimation::Pose CameraAnimation::InterpolateKeyframes(size_t prevIndex, size_t nextIndex, float t) const {
    const CameraKeyframe& prev = keyframes_[prevIndex];
    const CameraKeyframe& next = keyframes_[nextIndex];

    Vector3 position = EvaluateSegmentPosition(prevIndex, nextIndex, t);

    // 座標系が混在する場合は prev 側を優先。TARGET_RELATIVE では position をオフセットとしてターゲット位置に加算
    if (prev.coordinateType == CameraKeyframe::CoordinateType::TARGET_RELATIVE && targetTransform_) {
      position = Vec3::Add(targetTransform_->translate, position);
    }

    // 回転は Slerp
    Quaternion q1 = EulerToQuaternion(prev.rotation);
    Quaternion q2 = EulerToQuaternion(next.rotation);
    Quaternion qResult = Quat::Slerp(q1, q2, t);

    return {
      .position = position,
      .rotation = QuaternionToEuler(qResult),
      .fov      = Vec3::Lerp(prev.fov, next.fov, t),
    };
  }

  CameraAnimation::Pose CameraAnimation::KeyframePose(const CameraKeyframe& kf) const {
    Vector3 position = kf.position;
    if (kf.coordinateType == CameraKeyframe::CoordinateType::TARGET_RELATIVE && targetTransform_) {
      position = Vec3::Add(targetTransform_->translate, kf.position);
    }
    return { .position = position, .rotation = kf.rotation, .fov = kf.fov };
  }

  void CameraAnimation::ApplyPose(const Pose& pose) {
    if (!camera_) {
      return;
    }
    camera_->SetTranslate(pose.position);
    camera_->SetRotate(pose.rotation);
    camera_->SetFovY(pose.fov);
  }

  Vector3 CameraAnimation::EvaluateSegmentPosition(size_t prevIndex, size_t nextIndex, float easedT) const {
    const CameraKeyframe& prev = keyframes_[prevIndex];
    const CameraKeyframe& next = keyframes_[nextIndex];

    if (prev.pathType != CameraKeyframe::PathType::CATMULL_ROM || prevIndex == nextIndex) {
      return Vec3::Lerp(prev.position, next.position, easedT);
    }

    // 曲線の形を決めるには区間の外側の点 p0/p3 も必要。
    // 隣が無い・座標系が違うときは端の点で代用する
    Vector3 p0 = (prevIndex >= 1 && keyframes_[prevIndex - 1].coordinateType == prev.coordinateType)
      ? keyframes_[prevIndex - 1].position : prev.position;
    Vector3 p3 = (nextIndex + 1 < keyframes_.size() && keyframes_[nextIndex + 1].coordinateType == prev.coordinateType)
      ? keyframes_[nextIndex + 1].position : next.position;

    return Vec3::CatmullRom(p0, prev.position, next.position, p3, easedT);
  }

  CameraAnimation::Pose CameraAnimation::EvaluatePoseAtTime(float time) const {
    if (keyframes_.empty()) {
      return {};
    }

    size_t prevIndex = 0, nextIndex = 0;
    float t = 0.0f;
    if (ResolveSegmentAtTime(time, prevIndex, nextIndex, t)) {
      return InterpolateKeyframes(prevIndex, nextIndex, t);
    }
    return KeyframePose(keyframes_[0]);
  }

  float CameraAnimation::ApplyEasing(float t, const CameraKeyframe& kf) {
    return ApplyEasing(t, kf.interpolation, kf.bezierP1, kf.bezierP2);
  }

  float CameraAnimation::ApplyEasing(float t, CameraKeyframe::InterpolationType type, const Vector2& p1, const Vector2& p2) {
    switch (type) {
    case CameraKeyframe::InterpolationType::LINEAR:
      return Ease::Linear(t);

    case CameraKeyframe::InterpolationType::EASE_IN:
      return Ease::InQuad(t);

    case CameraKeyframe::InterpolationType::EASE_OUT:
      return Ease::OutQuad(t);

    case CameraKeyframe::InterpolationType::EASE_IN_OUT:
      return Ease::InOutQuad(t);

    case CameraKeyframe::InterpolationType::CUBIC_BEZIER:
      return Ease::CubicBezier(t, p1.x, p1.y, p2.x, p2.y);

    default:
      return t;
    }
  }

  Quaternion CameraAnimation::EulerToQuaternion(const Vector3& euler) const {
    Quaternion qx = Quat::MakeRotateAxisAngle(Vector3(1.0f, 0.0f, 0.0f), euler.x);
    Quaternion qy = Quat::MakeRotateAxisAngle(Vector3(0.0f, 1.0f, 0.0f), euler.y);
    Quaternion qz = Quat::MakeRotateAxisAngle(Vector3(0.0f, 0.0f, 1.0f), euler.z);

    // 回転順序: Y * X * Z
    Quaternion result = Quat::Multiply(qy, qx);
    result = Quat::Multiply(result, qz);

    return result;
  }

  Vector3 CameraAnimation::QuaternionToEuler(const Quaternion& q) const {
    Vector3 euler;

    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.x = std::atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (std::abs(sinp) >= 1.0f) {
      euler.y = std::copysignf(std::numbers::pi_v<float> / 2.0f, sinp); // ジンバルロック
    }
    else {
      euler.y = std::asinf(sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.z = std::atan2f(siny_cosp, cosy_cosp);

    return euler;
  }

  nlohmann::json CameraAnimation::ToJson() const {
    nlohmann::json json;
    json["duration"] = duration_;
    json["loop"] = isLooping_;
    json["play_speed"] = playSpeed_;

    json["start_mode"] = static_cast<int>(startMode_);
    json["blend_duration"] = blendDuration_;

    const char* timingModeStr = "PER_SEGMENT";
    if (timingMode_ == TimingMode::UNIFIED_WARP) {
      timingModeStr = "UNIFIED_WARP";
    }
    else if (timingMode_ == TimingMode::CONSTANT_SPEED) {
      timingModeStr = "CONSTANT_SPEED";
    }
    json["timing_mode"] = timingModeStr;
    json["global_easing"] = globalEasing_;
    json["global_bezier_p1"] = { globalBezierP1_.x, globalBezierP1_.y };
    json["global_bezier_p2"] = { globalBezierP2_.x, globalBezierP2_.y };

    json["keyframes"] = nlohmann::json::array();
    for (const auto& kf : keyframes_) {
      nlohmann::json kfJson = kf;
      kfJson["id"] = kf.id;
      json["keyframes"].push_back(std::move(kfJson));
    }
    return json;
  }

  void CameraAnimation::FromJson(const nlohmann::json& json) {
    isLooping_ = json.value("loop", false);
    playSpeed_ = json.value("play_speed", 1.0f);

    // 旧フォーマット互換: 欠落時はデフォルト値
    int startModeInt = json.value("start_mode", static_cast<int>(StartMode::JUMP_CUT));
    startMode_ = static_cast<StartMode>(startModeInt);
    blendDuration_ = json.value("blend_duration", kDefaultBlendDuration);

    std::string timingModeStr = json.value("timing_mode", "PER_SEGMENT");
    if (timingModeStr == "UNIFIED_WARP") {
      timingMode_ = TimingMode::UNIFIED_WARP;
    }
    else if (timingModeStr == "CONSTANT_SPEED") {
      timingMode_ = TimingMode::CONSTANT_SPEED;
    }
    else {
      timingMode_ = TimingMode::PER_SEGMENT;
    }

    if (json.contains("global_easing")) {
      globalEasing_ = json.at("global_easing").get<CameraKeyframe::InterpolationType>();
    }
    else {
      globalEasing_ = CameraKeyframe::InterpolationType::LINEAR;
    }

    if (json.contains("global_bezier_p1")) {
      auto p1 = json.at("global_bezier_p1");
      globalBezierP1_ = { p1[0].get<float>(), p1[1].get<float>() };
    }
    else {
      globalBezierP1_ = { 0.42f, 0.0f };
    }

    if (json.contains("global_bezier_p2")) {
      auto p2 = json.at("global_bezier_p2");
      globalBezierP2_ = { p2[0].get<float>(), p2[1].get<float>() };
    }
    else {
      globalBezierP2_ = { 0.58f, 1.0f };
    }

    keyframes_.clear();

    if (json.contains("keyframes")) {
      for (const auto& kfJson : json["keyframes"]) {
        CameraKeyframe& kf = keyframes_.emplace_back(kfJson.get<CameraKeyframe>());
        kf.id = kfJson.value("id", 0u);
        nextKeyframeId_ = std::max<uint32_t>(nextKeyframeId_, kf.id + 1);
      }
    }

    // Undo の復元は ToJson の id を持っているのでそのまま使い、ファイルから読んだものにだけ新しく振る
    for (CameraKeyframe& kf : keyframes_) {
      if (kf.id == 0) {
        kf.id = nextKeyframeId_++;
      }
    }

    SortKeyframes();
    UpdateDuration();
  }

  bool CameraAnimation::LoadFromJson(const std::string& filepath) {
    try {
      const std::filesystem::path jsonPath = ToFilePath(filepath);

      std::ifstream file(jsonPath);
      if (!file.is_open()) {
        return false;
      }

      nlohmann::json json;
      file >> json;
      file.close();

      FromJson(json);
      animationName_ = jsonPath.stem().string();

#ifdef _DEBUG
      LogToConsole(std::format("CameraAnimation: loaded '{}' from {}", animationName_, jsonPath.string()));
#endif

      return true;

    }
    catch (const std::exception& e) {
      (void)e;
      return false;
    }
  }

  bool CameraAnimation::SaveToJson(const std::string& filepath) const {
    try {
      const std::filesystem::path jsonPath = ToFilePath(filepath);
      std::filesystem::create_directories(jsonPath.parent_path());

      std::ofstream file(jsonPath);
      if (!file.is_open()) {
        return false;
      }

      nlohmann::json json = ToJson();
      for (nlohmann::json& kfJson : json["keyframes"]) {
        kfJson.erase("id");
      }
      file << json.dump(4);
      file.close();

#ifdef _DEBUG
      LogToConsole(std::format("CameraAnimation: saved '{}' to {}", jsonPath.stem().string(), jsonPath.string()));
#endif

      return true;

    }
    catch (const std::exception& e) {
      (void)e;
      return false;
    }
  }

} // namespace Tako
