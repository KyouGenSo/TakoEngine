#pragma once
#include "Vector2.h"
#include "Vector3.h"
#include <cstdint>
#include <json.hpp>

namespace Tako {

  /// <summary>
  /// カメラアニメーションのキーフレーム（時間軸上の1点でのカメラ状態）
  /// </summary>
  struct CameraKeyframe {
    enum class InterpolationType {
      LINEAR,
      EASE_IN,
      EASE_OUT,
      EASE_IN_OUT,
      CUBIC_BEZIER
    };

    enum class CoordinateType {
      WORLD,
      TARGET_RELATIVE ///< ターゲットからのオフセット
    };

    enum class PathType {
      LINEAR,
      CATMULL_ROM ///< 前後のキーフレームから接線を自動計算した滑らかな曲線（キーフレームは必ず通る）
    };

    float             time           = 0.0f;                       ///< 秒
    Vector3           position       = { 0.0f, 0.0f, 0.0f };       ///< WORLD では位置、TARGET_RELATIVE ではオフセット
    Vector3           rotation       = { 0.0f, 0.0f, 0.0f };       ///< オイラー角、ラジアン
    float             fov            = 0.45f;                      ///< ラジアン
    InterpolationType interpolation  = InterpolationType::LINEAR;  ///< このキーフレームから次への補間方法
    CoordinateType    coordinateType = CoordinateType::WORLD;
    PathType          pathType       = PathType::LINEAR;           ///< 次のキーフレームへ移動するときの経路の形（直線か曲線か）
    Vector2           bezierP1       = { 0.42f, 0.0f };            ///< CUBIC_BEZIER 用のカーブ制御点1（x:時間の割合 0～1, y:進行度）
    Vector2           bezierP2       = { 0.58f, 1.0f };            ///< CUBIC_BEZIER 用のカーブ制御点2
    uint32_t          id             = 0;                          ///< CameraAnimation が振る識別子。0 は未割り当て。ファイルには書かない

    CameraKeyframe() = default;

    CameraKeyframe(float t, const Vector3& pos, const Vector3& rot, float f,
      InterpolationType interp = InterpolationType::LINEAR,
      CoordinateType coordType = CoordinateType::WORLD)
      : time(t), position(pos), rotation(rot), fov(f), interpolation(interp), coordinateType(coordType) {
    }
  };

} // namespace Tako

// JSON 変換
namespace nlohmann {
  template <>
  struct adl_serializer<Tako::CameraKeyframe::CoordinateType> {
    static void to_json(json& j, const Tako::CameraKeyframe::CoordinateType& type) {
      switch (type) {
      case Tako::CameraKeyframe::CoordinateType::WORLD:
        j = "WORLD";
        break;
      case Tako::CameraKeyframe::CoordinateType::TARGET_RELATIVE:
        j = "TARGET_RELATIVE";
        break;
      }
    }

    static void from_json(const json& j, Tako::CameraKeyframe::CoordinateType& type) {
      std::string str = j.get<std::string>();
      if (str == "WORLD") {
        type = Tako::CameraKeyframe::CoordinateType::WORLD;
      }
      else if (str == "TARGET_RELATIVE") {
        type = Tako::CameraKeyframe::CoordinateType::TARGET_RELATIVE;
      }
      else {
        type = Tako::CameraKeyframe::CoordinateType::WORLD;
      }
    }
  };

  template <>
  struct adl_serializer<Tako::CameraKeyframe::InterpolationType> {
    static void to_json(json& j, const Tako::CameraKeyframe::InterpolationType& type) {
      switch (type) {
      case Tako::CameraKeyframe::InterpolationType::LINEAR:
        j = "LINEAR";
        break;
      case Tako::CameraKeyframe::InterpolationType::EASE_IN:
        j = "EASE_IN";
        break;
      case Tako::CameraKeyframe::InterpolationType::EASE_OUT:
        j = "EASE_OUT";
        break;
      case Tako::CameraKeyframe::InterpolationType::EASE_IN_OUT:
        j = "EASE_IN_OUT";
        break;
      case Tako::CameraKeyframe::InterpolationType::CUBIC_BEZIER:
        j = "CUBIC_BEZIER";
        break;
      }
    }

    static void from_json(const json& j, Tako::CameraKeyframe::InterpolationType& type) {
      std::string str = j.get<std::string>();
      if (str == "LINEAR") {
        type = Tako::CameraKeyframe::InterpolationType::LINEAR;
      }
      else if (str == "EASE_IN") {
        type = Tako::CameraKeyframe::InterpolationType::EASE_IN;
      }
      else if (str == "EASE_OUT") {
        type = Tako::CameraKeyframe::InterpolationType::EASE_OUT;
      }
      else if (str == "EASE_IN_OUT") {
        type = Tako::CameraKeyframe::InterpolationType::EASE_IN_OUT;
      }
      else if (str == "CUBIC_BEZIER") {
        type = Tako::CameraKeyframe::InterpolationType::CUBIC_BEZIER;
      }
      else {
        type = Tako::CameraKeyframe::InterpolationType::LINEAR;
      }
    }
  };

  template <>
  struct adl_serializer<Tako::CameraKeyframe::PathType> {
    static void to_json(json& j, const Tako::CameraKeyframe::PathType& type) {
      switch (type) {
      case Tako::CameraKeyframe::PathType::LINEAR:
        j = "LINEAR";
        break;
      case Tako::CameraKeyframe::PathType::CATMULL_ROM:
        j = "CATMULL_ROM";
        break;
      }
    }

    static void from_json(const json& j, Tako::CameraKeyframe::PathType& type) {
      std::string str = j.get<std::string>();
      if (str == "CATMULL_ROM") {
        type = Tako::CameraKeyframe::PathType::CATMULL_ROM;
      }
      else {
        type = Tako::CameraKeyframe::PathType::LINEAR;
      }
    }
  };

  template <>
  struct adl_serializer<Tako::CameraKeyframe> {
    static void to_json(json& j, const Tako::CameraKeyframe& keyframe) {
      j = json{
        {"time", keyframe.time},
        {"position", {keyframe.position.x, keyframe.position.y, keyframe.position.z}},
        {"rotation", {keyframe.rotation.x, keyframe.rotation.y, keyframe.rotation.z}},
        {"fov", keyframe.fov},
        {"interpolation", keyframe.interpolation},
        {"coordinateType", keyframe.coordinateType},
        {"pathType", keyframe.pathType},
        {"bezierP1", {keyframe.bezierP1.x, keyframe.bezierP1.y}},
        {"bezierP2", {keyframe.bezierP2.x, keyframe.bezierP2.y}}
      };
    }

    static void from_json(const json& j, Tako::CameraKeyframe& keyframe) {
      keyframe.time = j.at("time").get<float>();

      auto pos = j.at("position");
      keyframe.position.x = pos[0].get<float>();
      keyframe.position.y = pos[1].get<float>();
      keyframe.position.z = pos[2].get<float>();

      auto rot = j.at("rotation");
      keyframe.rotation.x = rot[0].get<float>();
      keyframe.rotation.y = rot[1].get<float>();
      keyframe.rotation.z = rot[2].get<float>();

      keyframe.fov = j.at("fov").get<float>();
      keyframe.interpolation = j.at("interpolation").get<Tako::CameraKeyframe::InterpolationType>();

      // 旧フォーマット互換: coordinateType 欠落時は WORLD
      if (j.contains("coordinateType")) {
        keyframe.coordinateType = j.at("coordinateType").get<Tako::CameraKeyframe::CoordinateType>();
      }
      else {
        keyframe.coordinateType = Tako::CameraKeyframe::CoordinateType::WORLD;
      }

      // 旧フォーマット互換: pathType/ベジェ制御点の欠落時はデフォルト値
      if (j.contains("pathType")) {
        keyframe.pathType = j.at("pathType").get<Tako::CameraKeyframe::PathType>();
      }
      else {
        keyframe.pathType = Tako::CameraKeyframe::PathType::LINEAR;
      }

      if (j.contains("bezierP1")) {
        auto p1 = j.at("bezierP1");
        keyframe.bezierP1 = { p1[0].get<float>(), p1[1].get<float>() };
      }
      else {
        keyframe.bezierP1 = { 0.42f, 0.0f };
      }

      if (j.contains("bezierP2")) {
        auto p2 = j.at("bezierP2");
        keyframe.bezierP2 = { p2[0].get<float>(), p2[1].get<float>() };
      }
      else {
        keyframe.bezierP2 = { 0.58f, 1.0f };
      }
    }
  };
}
