#pragma once
#include "Component.h"
#include "ObjectOverrides.h"
#include "Transform.h"

#include <memory>
#include <string>

namespace Tako {

  class Model;
  class Object3d;

  /// <summary>
  /// Actor の位置にモデルを描く（Unity の MeshFilter + MeshRenderer）。マテリアルと描画フラグは自分で持ち、シーンとプレハブに保存する。
  /// モデルの読み替えとマテリアルの解決は描画の直前に行う（GPU が使用中の物を消さないため）
  /// </summary>
  TCLASS(DisplayName = "Mesh")
  class MeshComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    MeshComponent();
    ~MeshComponent() override;

    /// <summary>
    /// 描画物の位置を Actor に合わせる。描画物に付けた Mesh エミッターが、同じフレームの位置で出せるようにする
    /// </summary>
    void LateTick(float deltaTime) override;

    void Draw() override;

    /// <summary>
    /// renderSettings（ObjectOverrides::Save の形）を読む
    /// </summary>
    void PostLoad(const nlohmann::json& raw) override;

    /// <summary>
    /// 描画設定があれば renderSettings として out に足す
    /// </summary>
    void PostSave(nlohmann::json& out) override;

#ifdef _DEBUG
    /// <summary>
    /// Rendering の欄（描画フラグと、マテリアル・値・テクスチャの上書き）
    /// </summary>
    bool DrawCustomImGui() override;
#endif

    //=====================================
    //Setter
    //=====================================
    void SetModel(const std::string& model) { model_ = model; }

    void SetVisible(bool visible) { visible_ = visible; }

    /// <summary>
    /// ファイルではなく、コードで作ったモデル（PrimitiveBuilder の球など）を描く。Model の欄は空にしておく
    /// </summary>
    void SetGeneratedModel(std::unique_ptr<Model> model);

    /// <summary>
    /// Actor からのずれ。揺らす演出など、Actor の位置を変えずに見た目だけ動かすのに使う
    /// </summary>
    void SetOffset(const Transform& offset) { offset_ = offset; }

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 描画物。色を変える・Mesh エミッターの形にするなど、スクリプトから使う。まだ読んでいなければここでモデルを読むので、BeginPlay からも使える
    /// </summary>
    Object3d* GetObject3d();

    bool IsVisible() const { return visible_; }

    const Transform& GetOffset() const { return offset_; }

  private: //非公開関数
    /// <summary>
    /// Model の欄が変わっていれば読み直す
    /// </summary>
    void EnsureLoaded();

    /// <summary>
    /// Offset と Actor のワールド行列を描画物に渡す。非表示の間も渡す（描画物の位置を使う物があるため）
    /// </summary>
    void SyncTransform();

  private: //メンバー変数
    TPROPERTY(Edit, Asset = "Model")
    std::string model_;
    TPROPERTY(Edit, DisplayName = "Offset")
    Transform   offset_  = { { 1.0f, 1.0f, 1.0f }, {}, {} };
    TPROPERTY(Edit, Tooltip = "オフなら描かない（影も落とさない）")
    bool        visible_ = true;

    std::unique_ptr<Object3d> object_;
    std::string               loadedModel_;
    ObjectOverrides           overrides_;              ///< object_ が SetOverrides で指す
    bool                      overridesDirty_ = true;  ///< 次の Draw でマテリアルを解決し直す
  };

} // namespace Tako
