#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"
#include "Transform.h"
#include "AnimationPlayer.h"
#include "DrawList.h"
#include "MaterialPropertyBlock.h"

namespace Tako {

class Model;
class Material;
struct ObjectOverrides;

/// <summary>
/// 3D オブジェクト基底クラス
/// Transform 管理とモデル描画機能。
///
/// 描画の設定（マテリアル・値・描画フラグ・メッシュの表示）は、項目ごとに次の中で一番上にあるものを毎フレーム使う。
///   1. コードの Set〇〇（std::nullopt を渡すと外す。SetMaterial は nullptr）
///   2. 同じ名前の ObjectSettings（UI で編集・保存する設定）
///   3. 既定（マテリアルの値・モデルの既定のマテリアル・DrawItem の初期値）
/// 値（色など）は描くときにマテリアルの同じ名前の値へ重ねるので、どのマテリアルで描いても効く
/// </summary>
class Object3d {

public: // 構造体
  // 座標変換行列データ（VP は Renderer が渡す視点 CB 側で掛ける）
  struct TransformationMatrix
  {
    Matrix4x4 world;
    Matrix4x4 worldInvTranspose;
  };

  /// <summary>
  /// コードで Set した上書き。外すまで ObjectSettings と既定より優先する
  /// </summary>
  struct CodeOverrides
  {
    std::optional<bool>                      transparent;
    std::optional<bool>                      castShadow;
    std::optional<int32_t>                   sortingOrder;
    std::optional<uint32_t>                  maskId;
    std::vector<std::shared_ptr<Material>>   materials;     ///< メッシュ番号順（nullptr は上書きなし）
    std::map<std::string, bool, std::less<>> meshVisible;   ///< メッシュ名ごと
    MaterialPropertyBlock                    properties;    ///< マテリアルの値とテクスチャ（"color"・"gTexture" など）
  };

public: // メンバー関数

  /// <summary>
  ///　コンストラクタ
  /// </summary>
  Object3d();

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~Object3d();

  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// 更新
  /// </summary>
  void Update();

  /// <summary>
  /// 描画リストへ登録する
  /// </summary>
  void Draw();

  /// <summary>
  /// デバッグ UI を表示
  /// </summary>
  void DrawImGui();

  /// <summary>
  /// 親 Object3d の特定の Joint にアタッチ
  /// </summary>
  /// <param name="parent">親となる Object3d</param>
  /// <param name="jointName">アタッチする Joint 名</param>
  /// <param name="offset">Joint からのオフセット</param>
  void AttachToJoint(Object3d* parent, const std::string& jointName, const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  /// <summary>
  /// 親 Joint からデタッチ
  /// </summary>
  void DetachFromJoint();

  //=======================================================
  //Setter
  //=======================================================
  /// <summary>
  /// モデルを設定
  /// </summary>
  /// <param name="fileName">モデルファイル名</param>
  void SetModel(const std::string& fileName);

  /// <summary>
  /// コード生成 Model を直接セット（PrimitiveBuilder 等）
  /// </summary>
  /// <param name="model">所有権を渡す Model（unique_ptr）</param>
  void SetModel(std::unique_ptr<Model> model);

  /// <summary>
  /// 名前を付ける。同じ名前の ObjectSettings（UI で編集・保存する描画設定）を次の描画から当てる
  /// </summary>
  void SetName(const std::string& name);

  void SetTransform(const Transform& transform) { transform_ = transform; }
  void SetScale(const Vector3& scale) { transform_.scale = scale; }
  void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
  void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

  /// <summary>
  /// マテリアルの "color" を上書きする（どのマテリアルで描いても、同じ名前の値に効く）
  /// </summary>
  /// <param name="color">RGBA（std::nullopt で上書きを外す）</param>
  void SetMaterialColor(const std::optional<Vector4>& color);

  /// <summary>
  /// 半透明描画モードを設定 (両面描画 + 深度書き込み無効で、半透明キューの奥から順に描く)
  /// </summary>
  /// <param name="isTransparent">半透明描画モードにする場合 true（std::nullopt で上書きを外す）</param>
  void SetTransparent(std::optional<bool> isTransparent) { code_.transparent = isTransparent; }

  void SetCastShadow(std::optional<bool> castShadow) { code_.castShadow = castShadow; }

  /// <summary>
  /// 同じ描画キューの中での描画順（小さいほど先。std::nullopt で上書きを外す）
  /// </summary>
  void SetSortingOrder(std::optional<int32_t> sortingOrder) { code_.sortingOrder = sortingOrder; }

  /// <summary>
  /// 同じ maskId のポストエフェクトをこのオブジェクトの画素だけに掛ける（0 で解除、1〜255。std::nullopt で上書きを外す）
  /// </summary>
  void SetMaskId(std::optional<uint32_t> maskId) { code_.maskId = maskId; }

  /// <summary>
  /// メッシュの描画に使うマテリアルを差し替える（nullptr で上書きを外す）
  /// </summary>
  /// <param name="meshIndex">モデル内のメッシュ番号</param>
  /// <param name="material">差し替えるマテリアル（複数の Object3d で共有してよい）</param>
  void SetMaterial(size_t meshIndex, std::shared_ptr<Material> material);

  /// <summary>
  /// UV トランスフォーム（"uvTransform"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="uvTransform">UV トランスフォーム情報</param>
  void SetUvTransform(const std::optional<Transform>& uvTransform);

  /// <summary>
  /// 環境マップテクスチャ（"gEnvironmentMap"）を設定する
  /// </summary>
  /// <param name="textureIndex">SRV インデックス（0 で上書きを外す）</param>
  void SetEnvironmentTexture(uint32_t textureIndex);

  /// <summary>
  /// 環境マップの係数（"envMapCoefficient"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="coefficient">環境マップ係数</param>
  void SetEnvMapCoefficient(std::optional<float> coefficient);

  /// <summary>
  /// 通常テクスチャ（"gTexture"）を差し替える
  /// </summary>
  /// <param name="fileName">"resources/Texture/" 相対名、または "EngineResources/" プレフィックス付きパス（空で上書きを外す）</param>
  void SetTexture(const std::string& fileName);

  /// <summary>
  /// 光沢度（"shininess"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="shininess">光沢度</param>
  void SetShininess(std::optional<float> shininess);

  /// <summary>
  /// ライティングの有効/無効（"enableLighting"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="enableLighting">ライティングを有効にするか</param>
  void SetEnableLighting(std::optional<bool> enableLighting);

  /// <summary>
  /// ハイライトの有効/無効（"enableHighlight"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="enableHighlight">ハイライトを有効にするか</param>
  void SetEnableHighlight(std::optional<bool> enableHighlight);

  /// <summary>
  /// 環境マップの有効/無効（"enableEnvMap"）を設定する（std::nullopt で上書きを外す）
  /// </summary>
  /// <param name="enableEnvMap">環境マップを有効にするか</param>
  void SetEnableEnvMap(std::optional<bool> enableEnvMap);

  /// <summary>
  /// メッシュ名を指定して表示・非表示を設定
  /// </summary>
  /// <param name="name">メッシュ名</param>
  /// <param name="visible">表示する場合 true（std::nullopt で上書きを外す）</param>
  void SetMeshVisible(const std::string& name, std::optional<bool> visible);

  /// <summary>
  /// コードで上書きするマテリアルの値（名前で Set / Clear する。カスタムシェーダーの値にも使える）
  /// </summary>
  MaterialPropertyBlock& GetMaterialProperties() { return code_.properties; }

  void SetAttachmentTranslate(const Vector3& offset) { attachmentOffset_.translate = offset; }
  void SetAttachmentRotate(const Vector3& offset) { attachmentOffset_.rotate = offset; }
  void SetAttachmentScale(const Vector3& offset) { attachmentOffset_.scale = offset; }
  void SetAttachmentTransform(const Transform& transform) { attachmentOffset_ = transform; }

  //=======================================================
  //Getter
  //=======================================================
  const Vector3& GetScale() const { return transform_.scale; }
  const Vector3& GetRotate() const { return transform_.rotate; }
  const Vector3& GetTranslate() const { return transform_.translate; }
  const Transform& GetTransform() const { return transform_; }

  Model* GetModel() const
  {
    return model_.get();
  }

  /// <summary>
  /// SetModel に渡したファイル名（Model を直接渡した場合は空）
  /// </summary>
  const std::string& GetModelName() const { return modelName_; }

  const std::string& GetName() const { return name_; }

#ifdef _DEBUG
  /// <summary>
  /// 生存中の Object3d（Object Settings エディタの一覧）
  /// </summary>
  static const std::vector<Object3d*>& GetLiveObjects();
#endif

  /// <summary>
  /// 最初のメッシュを描く色（コード ＞ ObjectSettings ＞ マテリアルの値。アニメーションの上乗せは含まない）
  /// </summary>
  /// <returns>マテリアルカラー（RGBA）</returns>
  Vector4 GetMaterialColor() const;

  /// <summary>
  /// コードで Set した色を除いた、最初のメッシュの色（ObjectSettings の Values ＞ 描くマテリアルの値）。点滅やフェードの基準に使う
  /// </summary>
  Vector4 GetBaseMaterialColor() const;

  // コード ＞ ObjectSettings ＞ 既定 で決まった値
  bool IsTransparent() const;
  bool CastsShadow() const;
  int32_t GetSortingOrder() const;
  uint32_t GetMaskId() const;

  /// <summary>
  /// メッシュ名を指定して表示状態を取得（コード ＞ ObjectSettings ＞ モデル）
  /// </summary>
  /// <param name="name">メッシュ名</param>
  /// <returns>表示中なら true</returns>
  bool IsMeshVisible(const std::string& name) const;

  /// <summary>
  /// コードで Set した上書き（Object Settings エディタの表示用）
  /// </summary>
  const CodeOverrides& GetCodeOverrides() const { return code_; }

  /// <summary>
  /// モデルが持つ全メッシュ名のリストを取得
  /// </summary>
  /// <returns>メッシュ名のリスト（モデル未設定なら空）</returns>
  std::vector<std::string> GetMeshNames() const;

  /// <summary>
  /// 親 Joint にアタッチ済みか取得（親と Joint 名の両方が有効なら true）
  /// </summary>
  bool IsAttached() const { return parentObject_ != nullptr && !parentJointName_.empty(); }

  /// <summary>
  /// ワールド行列を取得
  /// </summary>
  Matrix4x4 GetWorldMatrix() const;

  /// <summary>
  /// トランスフォームと色に上乗せするアニメーション。target が空のトラックだけが効く
  /// </summary>
  AnimationPlayer& GetAnimator() { return animator_; }

private: // プライベートメンバー関数
  /// <summary>
  /// 座標変換行列データの生成
  /// </summary>
  void CreateTransformationMatrixData();

  /// <summary>
  /// アニメーションの上乗せの色が変わったら、次の描画で "color" に掛け直させる
  /// </summary>
  void ApplyAnimationColor(const Vector4& color);

  /// <summary>
  /// 名前の ObjectSettings（名前が無い・設定が無ければ nullptr）
  /// </summary>
  const ObjectOverrides* FindSettings() const;

  /// <summary>
  /// メッシュのマテリアル（コード ＞ ObjectSettings ＞ モデルの既定）
  /// </summary>
  const Material& GetSlotMaterial(size_t meshIndex, const ObjectOverrides* settings) const;

  /// <summary>
  /// メッシュごとのマテリアル・値・表示を、コードの上書き・ObjectSettings・既定から作り直す
  /// </summary>
  void RebuildDrawSlots(const ObjectOverrides* settings);

private: // メンバー変数
  std::unique_ptr<Model> model_;
  std::string            modelName_;
  Transform              transform_;

  //バッファリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;

  Matrix4x4 finalWorldMatrix_;
  Matrix4x4 worldMatrix_;

  //バッファリソース内のデータを指すポインタ
  TransformationMatrix* transformationMatData_ = nullptr;

  //Joint アタッチメント用変数
  Object3d*   parentObject_       = nullptr;
  std::string parentJointName_;
  Transform   attachmentOffset_{};

  //描画の設定
  std::string   name_;
  uint32_t      nameId_ = 0;  ///< name_ の FNV-1a（0 は名前なし）
  CodeOverrides code_;

  //メッシュごとの描画の値（RebuildDrawSlots で作る。DrawItem がフレームの終わりまで指す）
  std::vector<MeshDrawSlot>              drawSlots_;
  std::vector<MaterialPropertyBlock>     slotProperties_;
  std::vector<std::shared_ptr<Material>> slotMaterials_;                  ///< drawSlots_ が指すマテリアルを、次に作り直すまで持つ（nullptr はモデルの既定）
  bool                                   slotsDirty_             = true;  ///< コードのマテリアル・表示・名前・モデル・アニメーションの色が変わった
  uint32_t                               builtSettingsVersion_   = 0;     ///< 作ったときの ObjectSettings のバージョン
  uint32_t                               builtPropertiesVersion_ = 0;     ///< 作ったときの code_.properties のバージョン

  //アニメーションの上乗せ
  AnimationPlayer animator_;
  Vector4         animColor_          = { 1.0f, 1.0f, 1.0f, 1.0f };  ///< "color" に掛ける上乗せの色
  bool            isAnimationVisible_ = true;
};

} // namespace Tako
