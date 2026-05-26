#pragma once
#include <unordered_map>
#include <any>
#include <string>
#include <optional>
#include "Vector3.h"

namespace Tako {

  /// <summary>
  /// ビヘイビアツリーのブラックボード。
  /// ノード間でデータを共有するための汎用ストレージ。
  /// std::any によりキーバリュー方式で任意型を保持する。
  /// </summary>
  class BTBlackboard {
  public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    BTBlackboard() = default;

    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~BTBlackboard() = default;

    //==========================================================================
    // 汎用値/オブジェクトアクセサ
    //==========================================================================

    /// <summary>
    /// 汎用値の設定。任意型を std::any として data_ に格納する。
    /// </summary>
    /// <typeparam name="T">データ型</typeparam>
    /// <param name="key">キー</param>
    /// <param name="value">値</param>
    template<typename T>
    void SetValue(const std::string& key, const T& value) {
      data_[key] = value;
    }

    /// <summary>
    /// 汎用値の取得。型不一致やキー不在は nullopt を返す。
    /// </summary>
    /// <typeparam name="T">データ型</typeparam>
    /// <param name="key">キー</param>
    /// <returns>値 (存在しない場合は nullopt)</returns>
    template<typename T>
    std::optional<T> GetValue(const std::string& key) const {
      auto it = data_.find(key);
      if (it != data_.end()) {
        try {
          return std::any_cast<T>(it->second);
        }
        catch (const std::bad_any_cast&) {
          return std::nullopt;
        }
      }
      return std::nullopt;
    }

    /// <summary>
    /// 汎用値の取得 (デフォルト値付き)。GetValue の value_or 簡易版。
    /// </summary>
    /// <typeparam name="T">データ型</typeparam>
    /// <param name="key">キー</param>
    /// <param name="defaultValue">取得失敗時のフォールバック</param>
    /// <returns>値</returns>
    template<typename T>
    T GetAs(const std::string& key, const T& defaultValue = T{}) const {
      return GetValue<T>(key).value_or(defaultValue);
    }

    /// <summary>
    /// 外部オブジェクトポインタの設定。所有権を持たない参照 (生ポインタ) を登録する用途。
    /// </summary>
    /// <typeparam name="T">対象型 (前方宣言のみで利用可)</typeparam>
    /// <param name="key">キー</param>
    /// <param name="ptr">登録するポインタ</param>
    template<typename T>
    void SetPtr(const std::string& key, T* ptr) {
      data_[key] = ptr;
    }

    /// <summary>
    /// 外部オブジェクトポインタの取得。
    /// </summary>
    /// <typeparam name="T">対象型 (前方宣言のみで利用可)</typeparam>
    /// <param name="key">キー</param>
    /// <returns>登録されたポインタ、未登録または型不一致なら nullptr</returns>
    template<typename T>
    T* GetPtr(const std::string& key) const {
      auto it = data_.find(key);
      if (it == data_.end()) {
        return nullptr;
      }
      try {
        return std::any_cast<T*>(it->second);
      }
      catch (const std::bad_any_cast&) {
        return nullptr;
      }
    }

    //==========================================================================
    // フレーム経過時間
    //==========================================================================

    /// <summary>
    /// フレームの経過時間を設定。
    /// </summary>
    /// <param name="deltaTime">経過時間 [秒]</param>
    void SetDeltaTime(float deltaTime) { deltaTime_ = deltaTime; }

    /// <summary>
    /// フレームの経過時間を取得。
    /// </summary>
    /// <returns>経過時間 [秒]</returns>
    float GetDeltaTime() const { return deltaTime_; }

    //==========================================================================
    // プリミティブ型ヘルパー
    //==========================================================================

    /// <summary>
    /// 整数値の設定。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="value">値</param>
    void SetInt(const std::string& key, int value) {
      data_[key] = value;
    }

    /// <summary>
    /// 整数値の取得。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="defaultValue">デフォルト値</param>
    /// <returns>値</returns>
    int GetInt(const std::string& key, int defaultValue = 0) const {
      return GetAs<int>(key, defaultValue);
    }

    /// <summary>
    /// 浮動小数点値の設定。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="value">値</param>
    void SetFloat(const std::string& key, float value) {
      data_[key] = value;
    }

    /// <summary>
    /// 浮動小数点値の取得。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="defaultValue">デフォルト値</param>
    /// <returns>値</returns>
    float GetFloat(const std::string& key, float defaultValue = 0.0f) const {
      return GetAs<float>(key, defaultValue);
    }

    /// <summary>
    /// ベクトル値の設定。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="value">値</param>
    void SetVector3(const std::string& key, const Vector3& value) {
      data_[key] = value;
    }

    /// <summary>
    /// ベクトル値の取得。
    /// </summary>
    /// <param name="key">キー</param>
    /// <param name="defaultValue">デフォルト値</param>
    /// <returns>値</returns>
    Vector3 GetVector3(const std::string& key, const Vector3& defaultValue = Vector3()) const {
      return GetAs<Vector3>(key, defaultValue);
    }

    //==========================================================================
    // キー操作
    //==========================================================================

    /// <summary>
    /// キーが存在するかチェック。
    /// </summary>
    /// <param name="key">キー</param>
    /// <returns>存在する場合 true</returns>
    bool HasKey(const std::string& key) const {
      return data_.find(key) != data_.end();
    }

    /// <summary>
    /// 特定キーの削除。
    /// </summary>
    /// <param name="key">キー</param>
    void RemoveKey(const std::string& key) {
      data_.erase(key);
    }

    /// <summary>
    /// 全データのクリア (deltaTime_ は据え置き)。
    /// </summary>
    void Clear() {
      data_.clear();
    }

  private:
    // フレームの経過時間 [秒]。最ホットパスなのでキーマップではなく専用フィールドに保持
    float deltaTime_ = 0.0f;

    // 汎用データストレージ (キー → std::any)
    std::unordered_map<std::string, std::any> data_;
  };

} // namespace Tako
