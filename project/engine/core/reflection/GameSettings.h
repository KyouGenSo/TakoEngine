#pragma once
#include "TObject.h"
#include "TypeRegistry.h"

#include <string>
#include <vector>

namespace Tako {

  /// <summary>
  /// ゲームが定義する調整値の入れ物（UE の UDeveloperSettings）。派生の型ごとに 1 つだけあり、
  /// resources/Json/GameSettings/型名.json に TPROPERTY を保存する。Debug では Engine Settings の Game 欄で編集する
  /// </summary>
  TCLASS()
  class TAKO_API GameSettings : public TObject {
    GENERATED_BODY()

  public: //メンバー関数
    /// <summary>
    /// T の唯一の値。初めて呼ばれたときに作ってファイルを読む
    /// </summary>
    template<class T>
    static T& Get()
    {
      static T& instance = static_cast<T&>(Get(T::StaticType()));
      return instance;
    }

    /// <summary>
    /// type の唯一の値。初めて呼ばれたときに作ってファイルを読む（type は GameSettings の作れる派生）
    /// </summary>
    static GameSettings& Get(const TypeInfo& type);

    /// <summary>
    /// TPROPERTY をファイルへ書く
    /// </summary>
    /// <returns>書けなければ false</returns>
    bool Save();

    /// <summary>
    /// ファイルから読み直す。ファイルが無いか読めなければ今の値のまま
    /// </summary>
    void Reload();

    /// <summary>
    /// type の値を捨てる。次の Get で作り直してファイルを読む。ゲームの DLL を外す前に呼ぶ（値の型は DLL の中にある）
    /// </summary>
    static void Release(const TypeInfo& type);

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 登録されている GameSettings の派生（名前順）
    /// </summary>
    static std::vector<const TypeInfo*> GetTypes();

    static std::string GetPath(const TypeInfo& type);

  protected: //メンバー関数
    GameSettings() = default;
  };

} // namespace Tako
