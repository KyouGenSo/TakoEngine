#pragma once

namespace Tako {
  class PropertyVisitor;
  struct TypeInfo;
}

// TakoHeaderTool（build/TakoHeaderTool.cs）が読む印。展開すると何も残らない
//   TCLASS()             : クラスの直前。TObject の派生にだけ付けられる
//   TSTRUCT()            : 値として持つ構造体の直前。継承できない。TCLASS / TSTRUCT の中にも書ける
//   TENUM()              : enum の直前。Inspector と保存で列挙子の名前を使う
//   TPROPERTY(指定子...) : メンバー変数の直前の行
//     型は bool / int32_t / uint32_t / float / Vector2〜4 / std::string / Transform / TENUM / TSTRUCT と、それらの std::vector
//     （vector の要素は配列の指定子をそのまま使う）
//     Edit / Visible      : Inspector で編集できる / 読み取り専用で出す（どちらも無ければ保存だけ）
//     Transient           : 保存しない
//     Category = "..."    : Inspector で同じ見出しにまとめる
//     ClampMin / ClampMax / Speed = 式 : ドラッグの範囲と速さ（式はクラスの中で評価される）
//     Color               : Vector3 / Vector4 を色として編集する
//     Angle               : ラジアンの float を度で編集する（ClampMin / ClampMax / Speed も度）
//     EnumAsInt           : enum を名前でなく数値で保存する
//     Tooltip / DisplayName = "..." : 説明と表示名（表示名の既定はメンバー名を単語に分けたもの）
#define TCLASS(...)
#define TSTRUCT(...)
#define TENUM(...)
#define TPROPERTY(...)

// TCLASS の本体の先頭に書く。定義は TakoHeaderTool が生成する
#define GENERATED_BODY()                                  \
  public:                                                 \
    static const ::Tako::TypeInfo& StaticType();          \
    const ::Tako::TypeInfo& GetType() const override;     \
    void Reflect(::Tako::PropertyVisitor& v) override;    \
  private:

// TSTRUCT の本体の先頭に書く。Reflect の定義は TakoHeaderTool が生成する（仮想にしないので構造体は大きくならない）
#define GENERATED_STRUCT_BODY()                           \
  public:                                                 \
    void Reflect(::Tako::PropertyVisitor& v);
