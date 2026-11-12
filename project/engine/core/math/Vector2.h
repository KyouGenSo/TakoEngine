#pragma once
#include <limits>
#include <cmath>

namespace Tako {

  /// <summary>
  /// 2次元ベクトル
  /// </summary>
  struct Vector2 final {
    float x;  ///< X 成分
    float y;  ///< Y 成分

    /// <summary>
    /// 加算代入演算子
    /// </summary>
    /// <param name="v">加算するベクトル</param>
    /// <returns>加算後の自身への参照</returns>
    Vector2& operator+=(const Vector2& v) {
      x += v.x;
      y += v.y;
      return *this;
    }

    /// <summary>
    /// 減算代入演算子
    /// </summary>
    /// <param name="v">減算するベクトル</param>
    /// <returns>減算後の自身への参照</returns>
    Vector2& operator-=(const Vector2& v) {
      x -= v.x;
      y -= v.y;
      return *this;
    }

    /// <summary>
    /// スカラー乗算代入演算子
    /// </summary>
    /// <param name="s">乗算するスカラー値</param>
    /// <returns>乗算後の自身への参照</returns>
    Vector2& operator*=(float s) {
      x *= s;
      y *= s;
      return *this;
    }

    /// <summary>
    /// スカラー除算代入演算子
    /// </summary>
    /// <param name="s">除算するスカラー値</param>
    /// <returns>除算後の自身への参照</returns>
    Vector2& operator/=(float s) {
      x /= s;
      y /= s;
      return *this;
    }

    /// <summary>
    /// 加算演算子
    /// </summary>
    /// <param name="v">加算するベクトル</param>
    /// <returns>加算結果の新しいベクトル</returns>
    Vector2 operator+(const Vector2& v) const {
      Vector2 result;
      result.x = x + v.x;
      result.y = y + v.y;

      return result;
    }

    /// <summary>
    /// 減算演算子
    /// </summary>
    /// <param name="v">減算するベクトル</param>
    /// <returns>減算結果の新しいベクトル</returns>
    Vector2 operator-(const Vector2& v) const {
      Vector2 result;
      result.x = x - v.x;
      result.y = y - v.y;

      return result;
    }

    /// <summary>
    /// スカラー乗算演算子
    /// </summary>
    /// <param name="s">乗算するスカラー値</param>
    /// <returns>乗算結果の新しいベクトル</returns>
    Vector2 operator*(float s) const {
      Vector2 result;
      result.x = x * s;
      result.y = y * s;

      return result;
    }

    /// <summary>
    /// スカラー乗算演算子(スカラーが左辺)
    /// </summary>
    /// <param name="s">乗算するスカラー値</param>
    /// <param name="v">乗算されるベクトル</param>
    /// <returns>乗算結果の新しいベクトル</returns>
    friend Vector2 operator*(float s, const Vector2& v) {
      return v * s;
    }

    /// <summary>
    /// スカラー除算演算子
    /// </summary>
    /// <param name="s">除算するスカラー値</param>
    /// <returns>除算結果の新しいベクトル</returns>
    Vector2 operator/(float s) const {
      Vector2 result;
      result.x = x / s;
      result.y = y / s;

      return result;
    }

    /// <summary>
    /// 単項マイナス演算子(符号反転)
    /// </summary>
    /// <returns>符号を反転した新しいベクトル</returns>
    Vector2 operator-() const {
      return { -x, -y };
    }

    /// <summary>
    /// 等価演算子(イプシロン比較)
    /// </summary>
    /// <param name="v">比較するベクトル</param>
    /// <returns>ほぼ等しい場合 true</returns>
    bool operator==(const Vector2& v) const {
      return (std::abs(x - v.x) <= std::numeric_limits<float>::epsilon() &&
        std::abs(y - v.y) <= std::numeric_limits<float>::epsilon());
    }

    /// <summary>
    /// 非等価演算子
    /// </summary>
    /// <param name="v">比較するベクトル</param>
    /// <returns>等しくない場合 true</returns>
    bool operator!=(const Vector2& v) const {
      return !(*this == v);
    }

    /// <summary>
    /// 正規化ベクトルを取得
    /// </summary>
    /// <returns>正規化されたベクトル(長さが1のベクトル)。ゼロベクトルの場合はゼロベクトルを返す</returns>
    Vector2 Normalize() const {
      Vector2 result;
      float length = std::sqrt(x * x + y * y);

      // ゼロベクトルの場合はそのまま返す
      if (length <= std::numeric_limits<float>::epsilon()) {
        result.x = 0.0f;
        result.y = 0.0f;
        return result;  // そのままゼロベクトルを返す
      }

      float invLength = 1.0f / length; // 除算回数を減らして最適化
      result = { x * invLength, y * invLength };
      return result;
    }

    /// <summary>
    /// ベクトルの長さ(大きさ)を取得
    /// </summary>
    /// <returns>ベクトルの長さ</returns>
    float Length() const {
      return std::sqrt(x * x + y * y);
    }

    /// <summary>
    /// ベクトルの長さの2乗を取得(平方根計算を省略)
    /// </summary>
    /// <returns>ベクトルの長さの2乗</returns>
    float LengthSquared() const {
      return x * x + y * y;
    }

    /// <summary>
    /// 内積を計算
    /// </summary>
    /// <param name="v">計算対象のベクトル</param>
    /// <returns>内積値</returns>
    float Dot(const Vector2& v) const {
      return x * v.x + y * v.y;
    }

    /// <summary>
    /// 2点間の距離を計算
    /// </summary>
    /// <param name="v">距離を計算する対象のベクトル</param>
    /// <returns>2点間の距離</returns>
    float Distance(const Vector2& v) const {
      float dx = x - v.x;
      float dy = y - v.y;
      return std::sqrt(dx * dx + dy * dy);
    }

    /// <summary>
    /// 2点間の距離の2乗を計算(平方根計算を省略)
    /// </summary>
    /// <param name="v">距離を計算する対象のベクトル</param>
    /// <returns>2点間の距離の2乗</returns>
    float DistanceSquared(const Vector2& v) const {
      float dx = x - v.x;
      float dy = y - v.y;
      return dx * dx + dy * dy;
    }

    /// <summary>
    /// 2つのベクトル間を線形補間
    /// </summary>
    /// <param name="a">開始ベクトル</param>
    /// <param name="b">終了ベクトル</param>
    /// <param name="t">補間係数(0.0〜1.0)</param>
    /// <returns>補間されたベクトル</returns>
    static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) {
      return {
        a.x + t * (b.x - a.x),
        a.y + t * (b.y - a.y)
      };
    }
  };

} // namespace Tako