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
    explicit JsonWriteVisitor(nlohmann::json& out) : stack_{ &out } {}

  protected: //非公開関数
    bool BeginStruct(const char* key, const PropertyMeta& meta) override;
    void EndStruct() override;
    bool BeginArray(const char* key, const PropertyMeta& meta, size_t& count) override;
    void EndArray() override;
    void Visit(const char* key, bool& value, const PropertyMeta& meta) override;
    void Visit(const char* key, int32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, float& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector2& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector3& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector4& value, const PropertyMeta& meta) override;
    void Visit(const char* key, std::string& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Transform& value, const PropertyMeta& meta) override;
    void Visit(const char* key, ActorRef& value, const PropertyMeta& meta) override;
    void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) override;

  private: //非公開関数
    /// <summary>
    /// key の書き込み先。key が nullptr（配列の要素）なら末尾に足した要素
    /// </summary>
    nlohmann::json& Slot(const char* key);

  private: //メンバー変数
    std::vector<nlohmann::json*> stack_;  ///< 書いている object / array。先頭が書き込み先の根
  };

  /// <summary>
  /// JSON のキーから TPROPERTY へ読む。キーが無いか型が合わなければ今の値のまま
  /// </summary>
  class JsonReadVisitor final : public PropertyVisitor {
  public: //メンバー関数
    explicit JsonReadVisitor(const nlohmann::json& in) : stack_{ { &in, 0 } } {}

  protected: //非公開関数
    bool BeginStruct(const char* key, const PropertyMeta& meta) override;
    void EndStruct() override;
    bool BeginArray(const char* key, const PropertyMeta& meta, size_t& count) override;
    void EndArray() override;
    void Visit(const char* key, bool& value, const PropertyMeta& meta) override;
    void Visit(const char* key, int32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, uint32_t& value, const PropertyMeta& meta) override;
    void Visit(const char* key, float& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector2& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector3& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Vector4& value, const PropertyMeta& meta) override;
    void Visit(const char* key, std::string& value, const PropertyMeta& meta) override;
    void Visit(const char* key, Transform& value, const PropertyMeta& meta) override;
    void Visit(const char* key, ActorRef& value, const PropertyMeta& meta) override;
    void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) override;

  private: //非公開関数
    /// <summary>
    /// 読むべき値。キーが無いか Transient なら nullptr。key が nullptr（配列の要素）なら次の要素
    /// </summary>
    const nlohmann::json* Find(const char* key, const PropertyMeta& meta);

  private: //構造体
    struct Frame {
      const nlohmann::json* node;
      size_t                nextIndex;  ///< 配列なら次に読む要素
    };

  private: //メンバー変数
    std::vector<Frame> stack_;
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
