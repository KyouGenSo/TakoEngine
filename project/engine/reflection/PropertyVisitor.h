#pragma once
#include "Transform.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace Tako {

  // PropertyMeta::flags。TPROPERTY の同じ名前の指定子から生成される
  inline constexpr uint32_t kPropEdit      = 1u << 0;
  inline constexpr uint32_t kPropVisible   = 1u << 1;
  inline constexpr uint32_t kPropTransient = 1u << 2;
  inline constexpr uint32_t kPropColor     = 1u << 3;
  inline constexpr uint32_t kPropEnumAsInt = 1u << 4;
  inline constexpr uint32_t kPropAngle     = 1u << 5;

  /// <summary>
  /// TENUM の列挙子 1 つ
  /// </summary>
  struct EnumEntry {
    const char* name;
    int64_t     value;
  };

  /// <summary>
  /// TENUM の列挙子の一覧。生成された Reflect の中に static で置かれる
  /// </summary>
  struct EnumInfo {
    std::span<const EnumEntry> entries;

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 値の名前。無ければ nullptr
    /// </summary>
    const char* FindName(int64_t value) const
    {
      for (const EnumEntry& entry : entries) {
        if (entry.value == value) { return entry.name; }
      }
      return nullptr;
    }

    std::optional<int64_t> FindValue(std::string_view name) const
    {
      for (const EnumEntry& entry : entries) {
        if (name == entry.name) { return entry.value; }
      }
      return std::nullopt;
    }
  };

  /// <summary>
  /// TPROPERTY の指定子
  /// </summary>
  struct PropertyMeta {
    const char*           displayName = "";
    const char*           category    = "";
    const char*           tooltip     = "";
    std::optional<double> min;
    std::optional<double> max;
    float                 speed       = 0.0f;     ///< 0 なら型ごとの既定
    uint32_t              flags       = 0;
    const EnumInfo*       enumInfo    = nullptr;  ///< TENUM の enum だけ
  };

  /// <summary>
  /// Reflect が TPROPERTY を 1 つずつ渡す相手。保存・読み込み・Inspector がそれぞれ派生する
  /// </summary>
  class PropertyVisitor {
  public: //メンバー関数
    virtual ~PropertyVisitor() = default;

    /// <summary>
    /// 生成された Reflect が呼ぶ入口。enum は int64_t にして渡す
    /// </summary>
    template<class T>
    void operator()(const char* key, T& value, const PropertyMeta& meta)
    {
      if constexpr (std::is_enum_v<T>) {
        int64_t raw = static_cast<int64_t>(value);
        VisitEnum(key, raw, meta);
        value = static_cast<T>(raw);
      } else {
        Visit(key, value, meta);
      }
    }

  protected: //非公開関数
    virtual void Visit(const char* key, bool& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, int32_t& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, float& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector2& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector3& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector4& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, std::string& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Transform& value, const PropertyMeta& meta) = 0;
    virtual void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) = 0;
  };

} // namespace Tako
