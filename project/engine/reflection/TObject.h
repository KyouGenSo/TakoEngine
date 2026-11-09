#pragma once
#include "Reflection.h"

#include <json_fwd.hpp>

namespace Tako {

  /// <summary>
  /// リフレクションの根クラス。TCLASS を付けるクラスはこれを継承する
  /// </summary>
  class TObject {
  public: //メンバー関数
    virtual ~TObject() = default;

    static const TypeInfo& StaticType();

    virtual const TypeInfo& GetType() const;

    /// <summary>
    /// TPROPERTY を訪問者へ渡す。派生の定義は TakoHeaderTool が生成し、親の Reflect を先に呼ぶ
    /// </summary>
    virtual void Reflect(PropertyVisitor& v);

    /// <summary>
    /// Inspector で値が変わった後に呼ばれる。他の値に合わせた補正を書く
    /// </summary>
    virtual void PostEditChange() {}

    /// <summary>
    /// JSON から値を読んだ後に呼ばれる。古いキーの読み替えなどに使う
    /// </summary>
    /// <param name="raw">読んだ JSON そのもの</param>
    virtual void PostLoad([[maybe_unused]] const nlohmann::json& raw) {}

    /// <summary>
    /// TPROPERTY を JSON に書いた後に呼ばれる。TPROPERTY で表せない中身（持っている別の TObject など）を足す
    /// </summary>
    /// <param name="out">書いた JSON。TPROPERTY が無ければ null</param>
    virtual void PostSave([[maybe_unused]] nlohmann::json& out) {}

#ifdef _DEBUG
    /// <summary>
    /// 自動の UI の後に描く追加の UI
    /// </summary>
    /// <returns>値を変えたら true</returns>
    virtual bool DrawCustomImGui() { return false; }
#endif

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// type かその派生なら true
    /// </summary>
    bool IsA(const TypeInfo& type) const;

    template<class T>
    bool IsA() const { return IsA(T::StaticType()); }
  };

} // namespace Tako
