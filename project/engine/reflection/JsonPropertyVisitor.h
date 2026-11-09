#pragma once
#include "PropertyVisitor.h"

#include <json.hpp>

namespace Tako {

  class TObject;

  /// <summary>
  /// TPROPERTY を JSON のキーへ書く。Transient は書かない
  /// </summary>
  class JsonWriteVisitor final : public PropertyVisitor {
  public: //メンバー関数
    /// <param name="out">書き込み先。null のままなら何も書かれなかった</param>
    explicit JsonWriteVisitor(nlohmann::json& out) : out_(out) {}

  protected: //非公開関数
    void Visit(const char* key, bool& value, const PropertyMeta& meta) override;
    void Visit(const char* key, int32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, float& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector2& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector3& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector4& value, const PropertyMeta& meta) override;
    void Visit(const char* key, std::string& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Transform& value, const PropertyMeta& meta) override;
    void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) override;

  private: //メンバー変数
    nlohmann::json& out_;
  };

  /// <summary>
  /// JSON のキーから TPROPERTY へ読む。キーが無いか型が合わなければ今の値のまま
  /// </summary>
  class JsonReadVisitor final : public PropertyVisitor {
  public: //メンバー関数
    explicit JsonReadVisitor(const nlohmann::json& in) : in_(in) {}

  protected: //非公開関数
    void Visit(const char* key, bool& value, const PropertyMeta& meta) override;
    void Visit(const char* key, int32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, float& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector2& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector3& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector4& value, const PropertyMeta& meta) override;
    void Visit(const char* key, std::string& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Transform& value, const PropertyMeta& meta) override;
    void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) override;

  private: //非公開関数
    /// <summary>
    /// 読むべき値。キーが無いか Transient なら nullptr
    /// </summary>
    const nlohmann::json* Find(const char* key, const PropertyMeta& meta) const;

  private: //メンバー変数
    const nlohmann::json& in_;
  };

  /// <summary>
  /// object の TPROPERTY を JSON にする。TPROPERTY が無ければ null
  /// </summary>
  nlohmann::json WriteProperties(TObject& object);

  /// <summary>
  /// JSON から object の TPROPERTY を読み、PostLoad を呼ぶ
  /// </summary>
  void ReadProperties(TObject& object, const nlohmann::json& in);

} // namespace Tako
