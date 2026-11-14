#pragma once
#include "ActorRef.h"
#include "ColorFunc.h"
#include "Gradient.h"
#include "TObject.h"
#include "Vector3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Tako {

  /// <summary>
  /// RunReflectionSelfTest が使う型。TakoHeaderTool が読みにくい書き方（{} 初期化、文字列の中の =、入れ子の enum / TSTRUCT、vector）を含める
  /// </summary>
  TCLASS()
  class TAKO_API ReflectionSelfTestObject : public TObject {
    GENERATED_BODY()

  public: //構造体
    TENUM()
    enum class Mode : uint8_t { First, Second = 5, Third };

    TSTRUCT()
    struct TAKO_API Entry {
      GENERATED_STRUCT_BODY()

      TPROPERTY(Edit)
      float weight = 1.0f;
      TPROPERTY(Edit)
      Mode mode = Mode::First;
      TPROPERTY(Edit)
      std::vector<int32_t> values;
    };

  public: //メンバー変数
    TPROPERTY(Edit, Category = "Test", ClampMin = 0, ClampMax = 10, Speed = 0.5)
    float value_ = 1.0f;
    TPROPERTY(Edit)
    int32_t count_{ 3 };
    TPROPERTY(Visible)
    bool isEnabled_ = true;
    TPROPERTY(Edit)
    Vector3 position_{ 1.0f, 2.0f, 3.0f };
    TPROPERTY(Edit, Tooltip = "a, b = c")
    std::string label_ = "a=b";
    TPROPERTY(Edit)
    Mode mode_ = Mode::Second;
    TPROPERTY(Edit, EnumAsInt)
    Mode modeAsInt_ = Mode::Third;
    TPROPERTY(Edit, Transient)
    float transient_ = 7.0f;
    TPROPERTY(Edit)
    Entry entry_;
    TPROPERTY(Edit)
    std::vector<Entry> entries_;
    TPROPERTY(Edit)
    std::vector<float> weights_{ 1.0f, 2.0f };
    TPROPERTY(Edit)
    std::vector<Mode> modes_;
    TPROPERTY(Edit)
    ActorRef target_;
    TPROPERTY(Edit)
    Color tint_ = Color::kWhite;
    TPROPERTY(Edit)
    Gradient gradient_;
  };

#ifdef _DEBUG
  /// <summary>
  /// 生成コードと JSON の往復を assert で確かめる。Debug の起動時に 1 回呼ぶ
  /// </summary>
  TAKO_API void RunReflectionSelfTest();
#endif

} // namespace Tako
