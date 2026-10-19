#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace Tako {

  /// <summary>
  /// 値を順に混ぜて 64 bit のハッシュを作る（FNV-1a）。保存内容が変わったかを、文字列にせずに調べるのに使う
  /// </summary>
  class Fnv1aHasher {
  private: //定数
    static constexpr uint64_t kPrime = 1099511628211ull;

  public: //メンバー関数
    void AddBytes(const void* data, size_t size) {
      const auto* bytes = static_cast<const unsigned char*>(data);
      for (size_t i = 0; i < size; ++i) {
        hash_ = (hash_ ^ bytes[i]) * kPrime;
      }
    }

    /// <summary>
    /// float・bool・Vector2 など詰め物の無い型は、バイト列のまま混ぜる
    /// </summary>
    template <class T>
      requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    void Add(const T& value) {
      AddBytes(&value, sizeof(value));
    }

    /// <summary>
    /// 長さも混ぜ、隣の文字列との区切りを変えた内容を区別する
    /// </summary>
    void Add(std::string_view value) {
      Add(value.size());
      AddBytes(value.data(), value.size());
    }

    //==============================
    //Getter
    //==============================
    uint64_t Get() const { return hash_; }

  private: //メンバー変数
    uint64_t hash_ = 14695981039346656037ull;
  };

} // namespace Tako
