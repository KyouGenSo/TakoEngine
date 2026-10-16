#pragma once
#include "Vector2.h"
#include "Vector4.h"
#include <memory>
#include <string>
#include <vector>

namespace Tako {

  class Sprite;

  /// <summary>
  /// UILayout の 1 要素。親の矩形に対してアンカーとピボットで配置する
  /// </summary>
  struct UIElement {
    UIElement();
    ~UIElement();

    //保存データ
    std::string name;
    std::string texture;                                   ///< TextureManager のキー。空なら描画しないグループ
    Vector2     anchorMin   = { 0.5f, 0.5f };              ///< 親矩形内の基準 (0..1)。Min と Max が異なる軸は親に合わせて伸縮する
    Vector2     anchorMax   = { 0.5f, 0.5f };
    Vector2     pivot       = { 0.5f, 0.5f };              ///< 自身の基準点 (0..1)。回転の中心も兼ねる
    Vector2     position    = {};                          ///< アンカー基準点から pivot までのずれ（基準解像度 px）
    Vector2     size        = { 100.0f, 100.0f };          ///< 伸縮しない軸はサイズ、伸縮する軸はアンカー幅との差分（基準解像度 px）
    float       rotation    = 0.0f;                        ///< 度数法。子には継承しない
    Vector4     color       = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector2     uvLeftTop   = {};                          ///< テクスチャ上の切り出し位置（px）
    Vector2     uvSize      = {};                          ///< テクスチャ上の切り出しサイズ（px）
    Vector4     sliceBorder = {};                          ///< 9-slice の枠幅（テクスチャ px、左上右下）
    bool        flipX       = false;
    bool        flipY       = false;
    bool        visible     = true;                        ///< false なら子も描画しない
    bool        locked      = false;                       ///< エディタ専用。true ならビューポートのクリックで選ばれない

    std::vector<std::unique_ptr<UIElement>> children;  ///< 並び順が描画順（後ろほど手前）

    //実行時データ
    UIElement*              parent   = nullptr;
    std::unique_ptr<Sprite> sprite;              ///< texture が空、または読み込めなかった場合は null
    Vector2                 rectMin  = {};       ///< 直近の Update で求めた画面矩形の左上（クライアント px）
    Vector2                 rectSize = {};       ///< 直近の Update で求めた画面矩形のサイズ（クライアント px）
  };

  /// <summary>
  /// JSON に保存した UI 要素ツリーを読み込み、画面サイズに合わせて配置・描画する
  /// </summary>
  class UILayout {
  public: //定数
    static constexpr const char* kDirectory = "resources/Json/UI/";

  private: //定数
    static constexpr Vector2 kDefaultReferenceResolution = { 1920.0f, 1080.0f };

  public: //メンバー関数
    UILayout();
    ~UILayout();
    UILayout(const UILayout&) = delete;
    UILayout& operator=(const UILayout&) = delete;

    /// <summary>
    /// 空のレイアウトにする。ファイルは Save で作られる
    /// </summary>
    /// <param name="name">レイアウト名（拡張子なし）</param>
    void Create(const std::string& name);

    /// <summary>
    /// kDirectory/name.json を読み込む。ファイルが無い場合は警告を出して空のレイアウトになり、Save で新規作成される
    /// </summary>
    /// <param name="name">レイアウト名（拡張子なし）</param>
    void Load(const std::string& name);

    /// <summary>
    /// 要素ツリーを Load したファイルへ保存する
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool Save() const;

    /// <summary>
    /// Save が書き込む内容（JSON 文字列）
    /// </summary>
    std::string Serialize() const;

    /// <summary>
    /// 現在のクライアントサイズから全要素の画面矩形を求め、スプライトへ反映する
    /// </summary>
    void Update();

    /// <summary>
    /// 表示中の要素を親→子の順に描画する。SpriteBasic::SetCommonRenderSetting の後に呼ぶ
    /// </summary>
    void Draw();

    /// <summary>
    /// 名前で要素を探す。エディタで削除・改名され得るので、ポインタは保持せず毎回引き直す
    /// </summary>
    /// <param name="name">要素名</param>
    /// <returns>見つからなければ nullptr</returns>
    UIElement* Find(const std::string& name);

    /// <summary>
    /// 画面矩形がこの値になるよう position と size を逆算する（アンカーとピボットは保つ）
    /// </summary>
    /// <param name="element">対象要素（ルート以外）</param>
    /// <param name="rectMin">画面矩形の左上（クライアント px）</param>
    /// <param name="rectSize">画面矩形のサイズ（クライアント px）</param>
    void SetScreenRect(UIElement& element, const Vector2& rectMin, const Vector2& rectSize) const;

    /// <summary>
    /// テクスチャを差し替え、切り出し範囲をテクスチャ全体に戻す。空文字ならスプライトを破棄してグループにする
    /// </summary>
    /// <param name="element">対象要素</param>
    /// <param name="texture">TextureManager のキー。未読み込みならここで読み込む</param>
    void SetTexture(UIElement& element, const std::string& texture);

    /// <summary>
    /// 末尾に子要素を追加する
    /// </summary>
    /// <param name="parent">追加先</param>
    /// <param name="name">名前の候補。重複していれば連番を付ける</param>
    /// <returns>追加した要素</returns>
    UIElement* AddChild(UIElement& parent, const std::string& name = "Element");

    /// <summary>
    /// 子孫ごと複製して直後に挿入する。複製側の名前は重複しないよう連番を付ける
    /// </summary>
    /// <param name="element">複製元（ルート以外）</param>
    /// <returns>複製した要素</returns>
    UIElement* Duplicate(const UIElement& element);

    /// <summary>
    /// 子孫ごと削除する
    /// </summary>
    /// <param name="element">削除する要素（ルート以外）</param>
    void Remove(UIElement& element);

    /// <summary>
    /// 移動後に newParent の子の index 番目になるよう移す。画面上の矩形は保つ
    /// </summary>
    /// <param name="element">移す要素（ルート以外）</param>
    /// <param name="newParent">移動先の親</param>
    /// <param name="index">移動後の位置（範囲外なら末尾）</param>
    /// <returns>自分の子孫の下へ移そうとした場合は false で何もしない</returns>
    bool MoveTo(UIElement& element, UIElement& newParent, size_t index);

    /// <summary>
    /// 未使用なら base、使用中なら base_1, base_2 … の最初の空き名を返す
    /// </summary>
    /// <param name="base">名前の候補</param>
    /// <returns>レイアウト内で重複しない名前</returns>
    std::string MakeUniqueName(const std::string& base);

    //==================================================
    //Getter
    //==================================================
    UIElement& GetRoot() { return root_; }
    const std::string& GetName() const { return name_; }
    float GetScale() const { return scale_; }

#ifdef _DEBUG
    /// <summary>
    /// 生存中のレイアウト一覧（エディタ用。生成順）
    /// </summary>
    /// <returns>生存中の全インスタンス</returns>
    static const std::vector<UILayout*>& GetInstances();
#endif

  private: //非公開関数
    void UpdateElement(UIElement& element);

    /// <summary>
    /// 親の rectMin/rectSize（直近の Update の値）から element の画面矩形を求める
    /// </summary>
    void ComputeRect(UIElement& element) const;

  private: //メンバー変数
    std::string name_;
    Vector2     referenceResolution_ = kDefaultReferenceResolution;  ///< position/size の基準となる画面サイズ
    float       scale_               = 1.0f;                         ///< 基準解像度 px → クライアント px
    UIElement   root_;                                               ///< 画面全体を表す見えないルート
  };

} // namespace Tako
