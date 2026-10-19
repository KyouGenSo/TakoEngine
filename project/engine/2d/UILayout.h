#pragma once
#include "Vector2.h"
#include "Vector4.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Tako {

  class Sprite;

  /// <summary>
  /// 9-slice の設定。テクスチャごとの共有値と、要素ごとの上書きの両方に使う
  /// </summary>
  struct UISlice {
    static constexpr int kRegionCount = 9;  ///< 区画の並びは行順（0:左上 … 4:中央 … 8:右下）

    Vector4                               border         = {};                                                        ///< 枠幅（テクスチャ px、左上右下）。切り出し範囲の各辺から測る
    std::array<bool, kRegionCount>        regionVisible  = { true, true, true, true, true, true, true, true, true };  ///< 中央を false にすると枠だけを描く
    std::array<std::string, kRegionCount> regionTextures;                                                             ///< 区画ごとの別画像のキー。空なら本体の画像のその区画。画像全体を区画いっぱいに引き伸ばす

    bool operator==(const UISlice&) const = default;
  };

  /// <summary>
  /// UILayout の 1 要素。親の矩形に対してアンカーとピボットで配置する
  /// </summary>
  struct UIElement {
    UIElement();
    ~UIElement();

    //保存データ
    std::string name;
    std::string texture;                                         ///< TextureManager のキー。空なら描画しないグループ
    Vector2     anchorMin         = { 0.5f, 0.5f };              ///< 親矩形内の基準 (0..1)。Min と Max が異なる軸は親に合わせて伸縮する
    Vector2     anchorMax         = { 0.5f, 0.5f };
    Vector2     pivot             = { 0.5f, 0.5f };              ///< 自身の基準点 (0..1)。回転の中心も兼ねる
    Vector2     position          = {};                          ///< アンカー基準点から pivot までのずれ（基準解像度 px）
    Vector2     size              = { 100.0f, 100.0f };          ///< 伸縮しない軸はサイズ、伸縮する軸はアンカー幅との差分（基準解像度 px）
    float       rotation          = 0.0f;                        ///< 度数法。子には継承しない
    Vector4     color             = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector2     uvLeftTop         = {};                          ///< テクスチャ上の切り出し位置（px）
    Vector2     uvSize            = {};                          ///< テクスチャ上の切り出しサイズ（px）
    bool        isSliced          = false;                       ///< false なら 9-slice を使わず、画像を要素の大きさに引き伸ばす
    bool        preserveAspect    = false;                       ///< isSliced でない間、切り出し範囲の縦横比を保って矩形に収める（枠と当たり判定は矩形のまま）
    UISlice     slice;                                           ///< この要素だけの 9-slice。isSliced と isSliceOverridden が両方 true の間だけ使う
    bool        isSliceOverridden = false;                       ///< isSliced の間、false ならテクスチャごとの共有の 9-slice を使う（無ければ枠なし）
    bool        flipX             = false;
    bool        flipY             = false;
    bool        visible           = true;                        ///< false なら子も描画しない
    bool        locked            = false;                       ///< エディタ専用。true ならビューポートのクリックで選ばれない

    std::vector<std::unique_ptr<UIElement>> children;  ///< 並び順が描画順（後ろほど手前）

    //実行時データ
    UIElement*              parent   = nullptr;
    std::unique_ptr<Sprite> sprite;              ///< texture が空、または読み込めなかった場合は null
    Vector2                 rectMin  = {};       ///< 直近の Update で求めた画面矩形の左上（クライアント px）
    Vector2                 rectSize = {};       ///< 直近の Update で求めた画面矩形のサイズ（クライアント px）
  };

  /// <summary>
  /// JSON に保存した UI 要素ツリーを読み込み、画面サイズに合わせて配置・描画する。
  /// 要素名で指定する Setter / Getter は、要素が見つからなければ Setter は何もせず、Getter は 0 / false / 空文字を返す
  /// </summary>
  class UILayout {
  public: //定数
    static constexpr const char* kDirectory        = "resources/Json/UI/";
    static constexpr const char* kTextureSlicePath = "resources/Json/TextureSlices.json";  ///< テクスチャごとの共有の 9-slice。kDirectory の中に置くとレイアウト一覧に混ざる

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
    /// Serialize が書き出した内容で要素ツリーを作り直す。名前と保存先は変えない
    /// </summary>
    void Deserialize(const std::string& text);

    /// <summary>
    /// Serialize の内容が同じなら同じになる値。Serialize より軽いので、毎フレームの変更の検出に使う
    /// </summary>
    uint64_t Hash() const;

    /// <summary>
    /// 現在のクライアントサイズから全要素の画面矩形を求め、スプライトへ反映する
    /// </summary>
    void Update();

    /// <summary>
    /// 表示中の要素を親→子の順に描画する。SpriteBasic::SetCommonRenderSetting の後に呼ぶ
    /// </summary>
    void Draw();

    /// <summary>
    /// name の要素とその子孫だけを描画する（ポストエフェクトの有無など、描画パスごとに分けて描くとき用）
    /// </summary>
    /// <param name="name">要素名。見つからなければ何も描かない</param>
    void Draw(const std::string& name);

    /// <summary>
    /// 名前で要素を探す。エディタで削除・改名され得るので、ポインタは保持せず毎回引き直す
    /// </summary>
    /// <param name="name">要素名</param>
    /// <returns>見つからなければ nullptr</returns>
    UIElement* Find(const std::string& name);
    const UIElement* Find(const std::string& name) const;

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
    /// 要素を子孫ごと保存形式（JSON 配列）にする。InsertElements で読み込める
    /// </summary>
    /// <param name="elements">書き出す要素（ルート以外）。並び順のまま書く</param>
    static std::string SerializeElements(const std::vector<const UIElement*>& elements);

    /// <summary>
    /// SerializeElements の内容を parent の子の index 番目から挿入する。名前が重なるものは連番を付ける
    /// </summary>
    /// <param name="parent">挿入先</param>
    /// <param name="serialized">SerializeElements の戻り値</param>
    /// <param name="index">最初の要素の位置（範囲外なら末尾）</param>
    /// <returns>挿入した要素（並び順）</returns>
    std::vector<UIElement*> InsertElements(UIElement& parent, const std::string& serialized, size_t index);

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

    /// <summary>
    /// テクスチャごとの共有の 9-slice を kTextureSlicePath から読み直す（未保存の変更は捨てる）。最初の参照時にも自動で読む
    /// </summary>
    static void LoadTextureSlices();

    /// <summary>
    /// テクスチャごとの共有の 9-slice を kTextureSlicePath へ保存する。既定値と同じものは書かない
    /// </summary>
    /// <returns>書き込めたら true</returns>
    static bool SaveTextureSlices();

    /// <summary>
    /// SaveTextureSlices が書き込む内容（JSON 文字列）
    /// </summary>
    static std::string SerializeTextureSlices();

    /// <summary>
    /// SerializeTextureSlices が書き出した内容で、テクスチャごとの共有の 9-slice を置き換える
    /// </summary>
    static void DeserializeTextureSlices(const std::string& text);

    /// <summary>
    /// SerializeTextureSlices の内容が同じなら同じになる値
    /// </summary>
    static uint64_t HashTextureSlices();

    //============================================================
    //Setter
    //============================================================
    void SetVisible(const std::string& name, bool visible);

    /// <summary>
    /// テクスチャを差し替え、切り出し範囲をテクスチャ全体に戻す。スプライトを作り直すので描画中には呼ばない
    /// </summary>
    void SetTexture(const std::string& name, const std::string& texture);

    void SetPosition(const std::string& name, const Vector2& position);
    void SetSize(const std::string& name, const Vector2& size);

    /// <summary>
    /// 回転（度数法）
    /// </summary>
    void SetRotation(const std::string& name, float rotation);

    void SetColor(const std::string& name, const Vector4& color);

    /// <summary>
    /// 色のアルファだけを変える
    /// </summary>
    void SetAlpha(const std::string& name, float alpha);

    /// <summary>
    /// アンカーの値だけを変える（画面上の矩形は保たない）
    /// </summary>
    void SetAnchor(const std::string& name, const Vector2& anchorMin, const Vector2& anchorMax);

    /// <summary>
    /// 伸縮しない 1 点のアンカーにする（anchorMin = anchorMax = anchor）。画面上の矩形は保たない
    /// </summary>
    void SetAnchor(const std::string& name, const Vector2& anchor);

    void SetPivot(const std::string& name, const Vector2& pivot);

    /// <summary>
    /// テクスチャ上の切り出し範囲（テクスチャ px）
    /// </summary>
    void SetUV(const std::string& name, const Vector2& leftTop, const Vector2& size);

    /// <summary>
    /// 9-slice の枠幅（テクスチャ px、左上右下）。要素を 9-slice にし、要素だけの上書きとして設定する（共有の 9-slice は使わなくなる）
    /// </summary>
    void SetSliceBorder(const std::string& name, const Vector4& sliceBorder);

    void SetFlip(const std::string& name, bool flipX, bool flipY);

    /// <summary>
    /// テクスチャごとの共有の 9-slice を設定する。このテクスチャを 9-slice で使い、上書きしていない全要素（他のレイアウトも含む）に効く
    /// </summary>
    static void SetTextureSlice(const std::string& texture, const UISlice& slice);

    //============================================================
    //Getter
    //============================================================
    UIElement& GetRoot() { return root_; }
    const std::string& GetName() const { return name_; }
    float GetScale() const { return scale_; }

    bool IsVisible(const std::string& name) const;
    std::string GetTexture(const std::string& name) const;
    Vector2 GetPosition(const std::string& name) const;
    Vector2 GetSize(const std::string& name) const;

    /// <summary>
    /// 回転（度数法）
    /// </summary>
    float GetRotation(const std::string& name) const;

    Vector4 GetColor(const std::string& name) const;
    float GetAlpha(const std::string& name) const;
    Vector2 GetAnchorMin(const std::string& name) const;
    Vector2 GetAnchorMax(const std::string& name) const;
    Vector2 GetPivot(const std::string& name) const;
    Vector2 GetUVLeftTop(const std::string& name) const;
    Vector2 GetUVSize(const std::string& name) const;

    /// <summary>
    /// 実際に使っている 9-slice の枠幅（9-slice でなければ 0、上書きしていなければテクスチャごとの共有の値）
    /// </summary>
    Vector4 GetSliceBorder(const std::string& name) const;
    bool IsFlipX(const std::string& name) const;
    bool IsFlipY(const std::string& name) const;

    /// <summary>
    /// 直近の Update で求めた画面矩形の左上（クライアント px）。クリックの当たり判定などに使う
    /// </summary>
    Vector2 GetScreenRectMin(const std::string& name) const;

    /// <summary>
    /// 直近の Update で求めた画面矩形のサイズ（クライアント px）
    /// </summary>
    Vector2 GetScreenRectSize(const std::string& name) const;

    /// <summary>
    /// テクスチャごとの共有の 9-slice。登録が無ければ既定値（枠なし）
    /// </summary>
    static const UISlice& GetTextureSlice(const std::string& texture);

    /// <summary>
    /// 要素が実際に使う 9-slice（9-slice でなければ枠なし、上書きしていなければテクスチャごとの共有の値）
    /// </summary>
    static const UISlice& GetEffectiveSlice(const UIElement& element);

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
