#pragma once
#include "TObject.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Tako {

  /// <summary>
  /// TCLASS 1 つ分の型情報。生成される StaticType() が static に持つ
  /// </summary>
  struct TypeInfo {
    using CreateFunc = std::unique_ptr<TObject>(*)();

    const char*     name;
    const TypeInfo* base;
    CreateFunc      create;            ///< 抽象クラスと既定コンストラクタの無いクラスは nullptr
    const char*     displayName = "";  ///< TCLASS(DisplayName = "...")。エディタの一覧に出す名前。空なら name を使う
    const char*     category    = "";  ///< TCLASS(Category = "...")。エディタの一覧の分類
    uint32_t        color       = 0;   ///< TCLASS(Color = 0xRRGGBB)。エディタで見分ける色。0 は指定なし
    const char*     required    = "";  ///< TCLASS(Requires = "型名")。同じ Actor に要るコンポーネントの型名
    int32_t         order       = 0;   ///< TCLASS(Order = n)。BeginPlay / Tick / LateTick を呼ぶ順（小さいほど先）

    /// <summary>
    /// other と同じ型かその派生なら true
    /// </summary>
    bool IsChildOf(const TypeInfo& other) const;
  };

  /// <summary>
  /// T を作る関数。作れない型は nullptr
  /// </summary>
  template<class T>
  constexpr TypeInfo::CreateFunc MakeTypeCreator()
  {
    if constexpr (std::is_abstract_v<T> || !std::is_default_constructible_v<T>) {
      return nullptr;
    } else {
      return []() -> std::unique_ptr<TObject> { return std::make_unique<T>(); };
    }
  }

  /// <summary>
  /// 型の名前から TypeInfo を引く表。保存したファイルの "type" から物を作るときに使う
  /// </summary>
  class TypeRegistry {
  public: //メンバー関数
    static TypeRegistry* GetInstance();

    /// <summary>
    /// 型を登録する。同じ名前の別の型があれば警告して上書きしない
    /// </summary>
    void Register(const TypeInfo& type);

    /// <summary>
    /// 名前から作る。未登録か作れない型なら nullptr
    /// </summary>
    std::unique_ptr<TObject> Create(std::string_view name) const;

    //=====================================
    //Getter
    //=====================================
    const TypeInfo* Find(std::string_view name) const;

    /// <summary>
    /// base の派生（base 自身を含む）のうち、作れるものを名前順に返す
    /// </summary>
    std::vector<const TypeInfo*> GetCreatableTypes(const TypeInfo& base) const;

  private: //メンバー変数
    std::map<std::string, const TypeInfo*, std::less<>> types_;
  };

  // 生成された TakoReflection.gen.cpp が定義する。TakoFramework::Initialize が両方呼ぶ
  void RegisterTypes_Engine();
  void RegisterTypes_Game();

} // namespace Tako
