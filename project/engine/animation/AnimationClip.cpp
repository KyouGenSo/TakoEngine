#include "AnimationClip.h"
#include "Logger.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <unordered_map>

namespace Tako {

  using json = nlohmann::json;

  namespace {

    constexpr EaseType kOutCubic = EaseType::OutCubic;

    // AnimProperty と同じ並び
    constexpr AnimPropertyInfo kPropertyInfos[] = {
      { "TranslateX", 0.0f, kOutCubic, false }, { "TranslateY", 0.0f, kOutCubic, false }, { "TranslateZ", 0.0f, kOutCubic, false },
      { "RotateX", 0.0f, kOutCubic, false }, { "RotateY", 0.0f, kOutCubic, false }, { "RotateZ", 0.0f, kOutCubic, false },
      { "ScaleX", 1.0f, kOutCubic, false }, { "ScaleY", 1.0f, kOutCubic, false }, { "ScaleZ", 1.0f, kOutCubic, false },
      { "ColorR", 1.0f, kOutCubic, false }, { "ColorG", 1.0f, kOutCubic, false }, { "ColorB", 1.0f, kOutCubic, false }, { "ColorA", 1.0f, kOutCubic, false },
      // Linear で 0 → コマ数 とつなぐと、全コマを同じ時間ずつ出す
      { "Frame", 0.0f, EaseType::Linear, false },
      { "Visible", 1.0f, EaseType::Constant, true },
    };
    static_assert(std::size(kPropertyInfos) == static_cast<size_t>(AnimProperty::Count));

    constexpr const char* kLoopModeNames[] = { "Once", "Loop", "PingPong" };
    static_assert(std::size(kLoopModeNames) == static_cast<size_t>(AnimationClip::LoopMode::Count));

    std::unordered_map<std::string, std::unique_ptr<AnimationClip>>& Clips() {
      static std::unordered_map<std::string, std::unique_ptr<AnimationClip>> clips;
      return clips;
    }

    std::filesystem::path FilePath(const std::string& name) {
      return std::filesystem::path(AnimationClip::kDirectory) / (name + ".json");
    }

    void LogWarning(const std::string& message) {
      Logger::Log(message + "\n");
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(message, DebugUIManager::LogType::Warning);
#endif
    }

  } // namespace

  const AnimPropertyInfo& GetAnimPropertyInfo(AnimProperty property) {
    const size_t index = static_cast<size_t>(property);
    return kPropertyInfos[index < std::size(kPropertyInfos) ? index : 0];
  }

  std::optional<AnimProperty> AnimPropertyFromString(std::string_view name) {
    for (size_t i = 0; i < std::size(kPropertyInfos); ++i) {
      if (name == kPropertyInfos[i].name) {
        return static_cast<AnimProperty>(i);
      }
    }
    return std::nullopt;
  }

  AnimationClip::AnimationClip(const std::string& name) : name_(name) {}

  AnimationClip* AnimationClip::Find(const std::string& name) {
    auto& clips = Clips();
    if (auto it = clips.find(name); it != clips.end()) {
      return it->second.get();
    }
    auto clip = std::make_unique<AnimationClip>(name);
    if (!clip->Load()) {
      return nullptr;
    }
    return clips.emplace(name, std::move(clip)).first->second.get();
  }

  AnimationClip* AnimationClip::Create(const std::string& name) {
    std::unique_ptr<AnimationClip>& slot = Clips()[name];
    if (!slot) {
      slot = std::make_unique<AnimationClip>(name);
    }
    // 再生中のプレイヤーが同じポインタを持っているので、作り直さずに中身だけ空にする
    slot->FromJson(json::object());
    return slot.get();
  }

  bool AnimationClip::Load() {
    const std::filesystem::path path = FilePath(name_);
    std::ifstream               file(path);
    if (!file) {
      LogWarning(std::format("AnimationClip: {} not found", path.string()));
      return false;
    }
    try {
      FromJson(json::parse(file));
    }
    catch (const json::exception& e) {
      LogWarning(std::format("AnimationClip '{}': failed to parse: {}", name_, e.what()));
      return false;
    }
    return true;
  }

  bool AnimationClip::Save() const {
    const std::filesystem::path path = FilePath(name_);
    std::error_code             ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path);
    if (!file) {
      LogWarning(std::format("AnimationClip: cannot open {} for writing", path.string()));
      return false;
    }
    file << ToJson(false).dump(2);
    return static_cast<bool>(file);
  }

  json AnimationClip::ToJson(bool includeIds) const {
    json root;
    root["duration"] = duration_;
    root["loop"]     = GetLoopModeName(loopMode_);

    json tracks = json::array();
    for (const AnimTrack& track : tracks_) {
      json node;
      node["target"]   = track.target;
      node["property"] = GetAnimPropertyInfo(track.property).name;
      if (track.property == AnimProperty::Frame) {
        node["columns"] = track.columns;
        node["rows"]    = track.rows;
      }
      json keys = json::array();
      for (const AnimKey& key : track.keys) {
        json keyNode = { { "time", key.time }, { "value", key.value }, { "ease", Ease::ToString(key.ease) } };
        if (key.ease == EaseType::CubicBezier) {
          keyNode["bezier"] = { key.bezierP1.x, key.bezierP1.y, key.bezierP2.x, key.bezierP2.y };
        }
        if (includeIds) {
          keyNode["id"] = key.id;
        }
        keys.push_back(std::move(keyNode));
      }
      node["keys"] = std::move(keys);
      tracks.push_back(std::move(node));
    }
    root["tracks"] = std::move(tracks);

    json events = json::array();
    for (const AnimEvent& event : events_) {
      json node = { { "time", event.time }, { "name", event.name } };
      if (includeIds) {
        node["id"] = event.id;
      }
      events.push_back(std::move(node));
    }
    root["events"] = std::move(events);
    return root;
  }

  void AnimationClip::FromJson(const json& root) {
    duration_ = (std::max)(root.value("duration", 1.0f), 0.0f);

    loopMode_ = LoopMode::Once;
    const std::string loop = root.value("loop", std::string(kLoopModeNames[0]));
    for (size_t i = 0; i < std::size(kLoopModeNames); ++i) {
      if (loop == kLoopModeNames[i]) {
        loopMode_ = static_cast<LoopMode>(i);
      }
    }

    tracks_.clear();
    events_.clear();

    auto readId = [&](const json& node, uint32_t& id) {
      id      = node.value("id", 0u);
      nextId_ = (std::max)(nextId_, id + 1);
    };

    if (root.contains("tracks")) {
      for (const json& node : root["tracks"]) {
        const std::string                 propertyName = node.value("property", std::string());
        const std::optional<AnimProperty> property     = AnimPropertyFromString(propertyName);
        if (!property) {
          LogWarning(std::format("AnimationClip '{}': unknown property '{}'", name_, propertyName));
          continue;
        }
        AnimTrack& track = tracks_.emplace_back();
        track.target   = node.value("target", std::string());
        track.property = *property;
        track.columns  = (std::max)(node.value("columns", 1), 1);
        track.rows     = (std::max)(node.value("rows", 1), 1);
        if (!node.contains("keys")) {
          continue;
        }
        for (const json& keyNode : node["keys"]) {
          AnimKey& key = track.keys.emplace_back();
          key.time  = keyNode.value("time", 0.0f);
          key.value = keyNode.value("value", 0.0f);
          key.ease  = Ease::FromString(keyNode.value("ease", std::string())).value_or(EaseType::Linear);
          if (keyNode.contains("bezier") && keyNode["bezier"].size() == 4) {
            const json& b = keyNode["bezier"];
            key.bezierP1 = { b[0].get<float>(), b[1].get<float>() };
            key.bezierP2 = { b[2].get<float>(), b[3].get<float>() };
          }
          readId(keyNode, key.id);
        }
      }
    }

    if (root.contains("events")) {
      for (const json& node : root["events"]) {
        AnimEvent& event = events_.emplace_back();
        event.time = node.value("time", 0.0f);
        event.name = node.value("name", std::string());
        readId(node, event.id);
      }
    }

    // Undo の復元は id を持っているのでそのまま使い、ファイルから読んだものにだけ新しく振る
    for (AnimTrack& track : tracks_) {
      for (AnimKey& key : track.keys) {
        key.id = key.id != 0 ? key.id : nextId_++;
      }
    }
    for (AnimEvent& event : events_) {
      event.id = event.id != 0 ? event.id : nextId_++;
    }

    SortByTime();
  }

  void AnimationClip::EvaluateInto(float time, const std::string& name, const std::string& boundTarget, AnimationPose& pose) const {
    for (const AnimTrack& track : tracks_) {
      const std::string& target = track.target.empty() ? boundTarget : track.target;
      if (track.keys.empty() || target != name) {
        continue;
      }
      const float value = EvaluateTrack(track, time);
      switch (track.property) {
      case AnimProperty::TranslateX: pose.translate.x += value; break;
      case AnimProperty::TranslateY: pose.translate.y += value; break;
      case AnimProperty::TranslateZ: pose.translate.z += value; break;
      case AnimProperty::RotateX:    pose.rotate.x += value; break;
      case AnimProperty::RotateY:    pose.rotate.y += value; break;
      case AnimProperty::RotateZ:    pose.rotate.z += value; break;
      case AnimProperty::ScaleX:     pose.scale.x *= value; break;
      case AnimProperty::ScaleY:     pose.scale.y *= value; break;
      case AnimProperty::ScaleZ:     pose.scale.z *= value; break;
      case AnimProperty::ColorR:     pose.color.x *= value; break;
      case AnimProperty::ColorG:     pose.color.y *= value; break;
      case AnimProperty::ColorB:     pose.color.z *= value; break;
      case AnimProperty::ColorA:     pose.color.w *= value; break;
      case AnimProperty::Frame:
        pose.frame   = (std::max)(static_cast<int>(std::floor(value)), 0);
        pose.columns = (std::max)(track.columns, 1);
        pose.rows    = (std::max)(track.rows, 1);
        break;
      case AnimProperty::Visible:
        pose.visible = pose.visible && value >= 0.5f;
        break;
      default:
        break;
      }
    }
  }

  float AnimationClip::WrapTime(float elapsed) const {
    if (duration_ <= 0.0f || elapsed <= 0.0f) {
      return 0.0f;
    }
    switch (loopMode_) {
    case LoopMode::Loop:
      return std::fmod(elapsed, duration_);
    case LoopMode::PingPong: {
      const float phase = std::fmod(elapsed, duration_ * 2.0f);
      return phase <= duration_ ? phase : duration_ * 2.0f - phase;
    }
    default:
      return (std::min)(elapsed, duration_);
    }
  }

  void AnimationClip::CollectEvents(float from, float to, std::vector<const AnimEvent*>& out) const {
    if (events_.empty() || to <= from || to < 0.0f) {
      return;
    }

    auto addIfPassed = [&](const AnimEvent& event, float at) {
      if (at > from && at <= to) {
        out.push_back(&event);
      }
    };

    if (loopMode_ == LoopMode::Once || duration_ <= 0.0f) {
      for (const AnimEvent& event : events_) {
        if (event.time <= duration_) {
          addIfPassed(event, event.time);
        }
      }
      return;
    }

    // 1 周ごとにイベントの時刻を経過秒へ写して判定する。PingPong の 1 周は往復ぶん
    const bool  isPingPong = loopMode_ == LoopMode::PingPong;
    const float period     = isPingPong ? duration_ * 2.0f : duration_;
    const int   lastCycle  = static_cast<int>(std::floor(to / period));
    // ponytail: 1 フレームで何周も進んだ場合は最後の 2 周だけ見る。全周分の発火が要るなら上限を外す
    const int firstCycle = (std::max)(static_cast<int>(std::floor((std::max)(from, 0.0f) / period)), lastCycle - 1);
    for (int cycle = firstCycle; cycle <= lastCycle; ++cycle) {
      const float cycleStart = period * static_cast<float>(cycle);
      for (const AnimEvent& event : events_) {
        if (event.time > duration_) {
          continue;
        }
        addIfPassed(event, cycleStart + event.time);
      }
      if (!isPingPong) {
        continue;
      }
      // 戻りの区間は逆順に通る。両端は行きと次の周で数えるので除く
      for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
        if (it->time > 0.0f && it->time < duration_) {
          addIfPassed(*it, cycleStart + period - it->time);
        }
      }
    }
  }

  void AnimationClip::SortByTime() {
    for (AnimTrack& track : tracks_) {
      std::ranges::stable_sort(track.keys, {}, &AnimKey::time);
    }
    std::ranges::stable_sort(events_, {}, &AnimEvent::time);
  }

  AnimKey* AnimationClip::FindKey(uint32_t id, size_t* trackIndex) {
    for (size_t i = 0; i < tracks_.size(); ++i) {
      for (AnimKey& key : tracks_[i].keys) {
        if (key.id == id) {
          if (trackIndex) {
            *trackIndex = i;
          }
          return &key;
        }
      }
    }
    return nullptr;
  }

  AnimEvent* AnimationClip::FindEvent(uint32_t id) {
    const auto it = std::ranges::find(events_, id, &AnimEvent::id);
    return it != events_.end() ? &*it : nullptr;
  }

  float AnimationClip::EvaluateTrack(const AnimTrack& track, float time) {
    const std::vector<AnimKey>& keys = track.keys;
    if (keys.empty()) {
      return GetAnimPropertyInfo(track.property).restValue;
    }
    if (time <= keys.front().time) {
      return keys.front().value;
    }
    if (time >= keys.back().time) {
      return keys.back().value;
    }

    const auto     next = std::ranges::upper_bound(keys, time, {}, &AnimKey::time);
    const AnimKey& to   = *next;
    const AnimKey& from = *(next - 1);
    const float    span = to.time - from.time;
    if (GetAnimPropertyInfo(track.property).isStep || from.ease == EaseType::Constant || span <= 0.0f) {
      return from.value;
    }
    const float t = Ease::Evaluate(from.ease, (time - from.time) / span, from.bezierP1.x, from.bezierP1.y, from.bezierP2.x, from.bezierP2.y);
    return from.value + (to.value - from.value) * t;
  }

  void AnimationClip::SetDuration(float duration) {
    duration_ = (std::max)(duration, 0.0f);
  }

  const char* AnimationClip::GetLoopModeName(LoopMode mode) {
    const size_t index = static_cast<size_t>(mode);
    return kLoopModeNames[index < std::size(kLoopModeNames) ? index : 0];
  }

} // namespace Tako
