#pragma once

#ifdef _DEBUG

#include "PropertyVisitor.h"

namespace Tako {

  class TObject;

  /// <summary>
  /// TPROPERTY の Edit / Visible を ImGui の入力欄にする。Category が変わるたびに見出しを出す
  /// </summary>
  class ImGuiPropertyVisitor final : public PropertyVisitor {
  public: //メンバー関数
    //=====================================
    //Getter
    //=====================================
    bool IsChanged() const { return changed_; }

    bool HasDrawn() const { return hasDrawn_; }

  protected: //非公開関数
    bool BeginStruct(const char* key, const PropertyMeta& meta) override;
    void EndStruct() override;

    /// <summary>
    /// 「名前 (個数)」の見出しと + / - を出す。+ / - で count を変える
    /// </summary>
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

    /// <summary>
    /// 今のシーンの Actor の一覧から選ぶ。Hierarchy の行をドロップしても入る
    /// </summary>
    void Visit(const char* key, ActorRef& value, const PropertyMeta& meta) override;
    void VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta) override;

  private: //非公開関数
    /// <summary>
    /// 見出しと ID を積み、読み取り専用なら無効化する
    /// </summary>
    /// <returns>Inspector に出さないなら false（End を呼ばない）</returns>
    bool Begin(const char* key, const PropertyMeta& meta);

    /// <summary>
    /// 直前の入力欄に説明を付け、Begin で積んだものを戻す
    /// </summary>
    void End(const PropertyMeta& meta, bool changed);

    /// <summary>
    /// 開いた TreeNode の中へ入る。中の Category の見出しは外と別に数える
    /// </summary>
    void EnterNested();

    /// <summary>
    /// EnterNested と Begin で積んだものを戻す
    /// </summary>
    void LeaveNested();

    /// <summary>
    /// Asset 指定子の文字列を、種類のファイルの一覧・Assets からのドロップ・Open で編集する
    /// </summary>
    /// <returns>値が変わったら true</returns>
    bool DrawAsset(std::string& value, const PropertyMeta& meta);

  private: //メンバー変数
    const char*              category_         = "";
    const char*              label_            = "";     ///< Begin が決めた入力欄の名前。配列の要素は "[i]"
    char                     indexLabel_[16]{};
    std::vector<int>         indices_;                   ///< 開いている配列ごとの、次の要素の番号
    std::vector<const char*> nestedCategories_;          ///< 入る前の category_
    bool                     changed_          = false;
    bool                     hasDrawn_         = false;
  };

  /// <summary>
  /// object の TPROPERTY と DrawCustomImGui を描き、値が変われば PostEditChange を呼ぶ。出す TPROPERTY が無ければその旨を出す
  /// </summary>
  /// <returns>値を変えたら true</returns>
  bool DrawProperties(TObject& object);

} // namespace Tako

#endif // _DEBUG
