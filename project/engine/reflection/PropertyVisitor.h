#pragma once
#include "ActorRef.h"
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
#include <vector>

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
    const char*           asset       = "";       ///< アセットの種類（AssetBrowser.h の kind）。空でなければ文字列をアセットの欄で編集する
  };

  class PropertyVisitor;

  template<class T>
  struct IsStdVector : std::false_type {};

  template<class T, class Allocator>
  struct IsStdVector<std::vector<T, Allocator>> : std::true_type {};

  /// <summary>
  /// 生成された Reflect を持つ型（TSTRUCT）
  /// </summary>
  template<class T>
  concept ReflectedStruct = requires(T& value, PropertyVisitor& visitor) { value.Reflect(visitor); };

  /// <summary>
  /// Reflect が TPROPERTY を 1 つずつ渡す相手。保存・読み込み・Inspector がそれぞれ派生する
  /// </summary>
  class PropertyVisitor {
  public: //メンバー関数
    virtual ~PropertyVisitor() = default;

    /// <summary>
    /// 生成された Reflect が呼ぶ入口。enum は int64_t にして渡し、TSTRUCT と std::vector は中へ降りる
    /// </summary>
    /// <param name="key">保存キー。nullptr なら配列の要素</param>
    template<class T>
    void operator()(const char* key, T& value, const PropertyMeta& meta)
    {
      if constexpr (std::is_enum_v<T>) {
        int64_t raw = static_cast<int64_t>(value);
        VisitEnum(key, raw, meta);
        value = static_cast<T>(raw);
      } else if constexpr (IsStdVector<T>::value) {
        VisitVector(key, value, meta);
      } else if constexpr (ReflectedStruct<T>) {
        if (BeginStruct(key, meta)) {
          value.Reflect(*this);
          EndStruct();
        }
      } else {
        Visit(key, value, meta);
      }
    }

  protected: //非公開関数
    /// <summary>
    /// TSTRUCT の中へ入る。false なら中身を渡さない（EndStruct も呼ばない）
    /// </summary>
    virtual bool BeginStruct(const char* key, const PropertyMeta& meta) = 0;
    virtual void EndStruct() = 0;

    /// <summary>
    /// std::vector の中へ入る。count を書き換えると要素数がその数になる
    /// </summary>
    /// <returns>false なら要素を渡さない（EndArray も呼ばない）</returns>
    virtual bool BeginArray(const char* key, const PropertyMeta& meta, size_t& count) = 0;
    virtual void EndArray() = 0;

    virtual void Visit(const char* key, bool& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, int32_t& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, float& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector2& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector3& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Vector4& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, std::string& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, Transform& value, const PropertyMeta& meta) = 0;
    virtual void Visit(const char* key, ActorRef& value, const PropertyMeta& meta) = 0;
    virtual void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) = 0;

  private: //非公開関数
    template<class T>
    void VisitVector(const char* key, std::vector<T>& values, const PropertyMeta& meta)
    {
      static_assert(!std::is_same_v<T, bool>, "std::vector<bool> は要素の参照を取れないので TPROPERTY にできない");
      size_t     count  = values.size();
      const bool isOpen = BeginArray(key, meta, count);
      // 増えた分は最後の要素の写しにする（Unity の + と同じ）
      if (count > values.size() && !values.empty()) {
        const T last = values.back();
        values.resize(count, last);
      } else {
        values.resize(count);
      }
      if (!isOpen) { return; }
      for (T& element : values) {
        (*this)(nullptr, element, meta);
      }
      EndArray();
    }
  };

} // namespace Tako
