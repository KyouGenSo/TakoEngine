#pragma once
#include "Component.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Tako {

  class UILayout;

  /// <summary>
  /// UI レイアウト（UI Editor で作るもの）を画面に出す（Unity の Canvas、UE の HUD に置く Widget）
  /// </summary>
  TCLASS()
  class UILayoutComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    UILayoutComponent();
    ~UILayoutComponent() override;

    /// <summary>
    /// レイアウトが変わっていれば読み直し、更新して描く（読み直しは描画の前なので、使用中のスプライトを消さない）
    /// </summary>
    void Draw() override;

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 要素を探して表示を切り替えるなど、スクリプトから使う。まだ読んでいなければここで読むので、BeginPlay からも使える
    /// </summary>
    UILayout& GetLayout();

    bool IsVisible() const { return visible_; }

    //=====================================
    //Setter
    //=====================================
    void SetVisible(bool visible) { visible_ = visible; }

  private: //非公開関数
    /// <summary>
    /// レイアウトの名前が変わっていれば読み直す
    /// </summary>
    void EnsureLoaded();

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "UILayout")
    std::string layout_;
    TPROPERTY(Edit, Tooltip = "他のレイアウトとの重なり順。大きいほど手前（同じ値なら、シーンの並びで後ろの Actor が手前）")
    int32_t     sortingOrder_ = 0;
    TPROPERTY(Edit, Tooltip = "オフなら描かない（ポーズのメニューなど、スクリプトが出すもの）")
    bool        visible_      = true;

    std::unique_ptr<UILayout> instance_;
    std::string               loadedLayout_;
  };

} // namespace Tako
