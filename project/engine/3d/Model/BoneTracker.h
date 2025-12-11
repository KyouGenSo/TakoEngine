#pragma once
#include "Model.h"
#include "EmitterManager.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace Tako {

/// <summary>
/// ボーンとエミッターのリンク情報構造体
/// モデルの特定ボーンにエミッターを追従させるための関連付けデータ
/// </summary>
struct BoneEmitterLink
{
  std::string boneName;       ///< 追跡対象のボーン名
  std::string emitterName;    ///< EmitterManager内で管理されているエミッター名
  int32_t boneIndex;          ///< ボーンインデックス（検索キャッシュ用、-1=未解決）
  Vector3 offset;             ///< ボーン位置からの相対オフセット
  bool isActive;              ///< このリンクがアクティブかどうか
};

/// <summary>
/// ボーントラッキングシステムクラス
/// スケルタルアニメーション対応モデルのボーンにエミッターを追従させる機能を提供
/// 既存のEmitterManagerを活用し、ボーン位置の自動更新を実現
/// キャラクターの手足や武器装着位置などへのエフェクト配置に有用
/// </summary>
class BoneTracker
{
public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  BoneTracker();

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~BoneTracker();

  /// <summary>
  /// 初期化（対象モデルとEmitterManagerを設定）
  /// </summary>
  /// <param name="model">対象モデル</param>
  /// <param name="emitterManager">エミッター管理システム</param>
  void Initialize(Model* model, EmitterManager* emitterManager);

  /// <summary>
  /// ボーンとエミッターをリンク
  /// </summary>
  /// <param name="linkName">リンクの識別名</param>
  /// <param name="boneName">追跡するボーン名</param>
  /// <param name="emitterName">EmitterManager内のエミッター名</param>
  /// <param name="offset">ボーンからのオフセット位置</param>
  void LinkBoneToEmitter(const std::string& linkName,
    const std::string& boneName,
    const std::string& emitterName,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  /// <summary>
  /// エミッターを作成してボーンにリンク（球体エミッター）
  /// </summary>
  /// <param name="linkName">リンクの識別名</param>
  /// <param name="boneName">追跡するボーン名</param>
  /// <param name="radius">球体の半径</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">生成頻度</param>
  /// <param name="offset">ボーンからのオフセット位置</param>
  void CreateAndLinkSphereEmitter(const std::string& linkName,
    const std::string& boneName,
    float radius, uint32_t count, float frequency,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  /// <summary>
  /// エミッターを作成してボーンにリンク（箱型エミッター）
  /// </summary>
  /// <param name="linkName">リンクの識別名</param>
  /// <param name="boneName">追跡するボーン名</param>
  /// <param name="size">ボックスのサイズ</param>
  /// <param name="rotation">ボックスの回転</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">生成頻度</param>
  /// <param name="offset">ボーンからのオフセット位置</param>
  void CreateAndLinkBoxEmitter(const std::string& linkName,
    const std::string& boneName,
    const Vector3& size, const Vector3& rotation,
    uint32_t count, float frequency,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  /// <summary>
  /// ボーン位置を取得してエミッター位置を更新
  /// </summary>
  /// <param name="transform">モデルのトランスフォーム</param>
  void Update(const Transform& transform);

  /// <summary>
  /// リンクの有効/無効を切り替え
  /// </summary>
  /// <param name="linkName">リンク名</param>
  /// <param name="active">有効フラグ</param>
  void SetLinkActive(const std::string& linkName, bool active);

  /// <summary>
  /// リンクのオフセットを変更
  /// </summary>
  /// <param name="linkName">リンク名</param>
  /// <param name="offset">新しいオフセット</param>
  void SetLinkOffset(const std::string& linkName, const Vector3& offset);

  /// <summary>
  /// リンクを削除
  /// </summary>
  /// <param name="linkName">削除するリンク名</param>
  void RemoveLink(const std::string& linkName);

  /// <summary>
  /// 全てのリンクを削除
  /// </summary>
  void ClearLinks();

  /// <summary>
  /// リンクが存在するか確認
  /// </summary>
  /// <param name="linkName">リンク名</param>
  /// <returns>存在する場合true</returns>
  bool HasLink(const std::string& linkName) const;

  /// <summary>
  /// リンク数を取得
  /// </summary>
  /// <returns>リンクの総数</returns>
  size_t GetLinkCount() const { return links_.size(); }

private:
  /// <summary>
  /// ボーンインデックスを検索してキャッシュ
  /// </summary>
  /// <param name="boneName">ボーン名</param>
  /// <returns>ボーンインデックス（見つからない場合-1）</returns>
  int32_t FindBoneIndex(const std::string& boneName);

private:
  Model* model_; ///< 追跡対象のスケルタルアニメーションモデル

  EmitterManager* emitterManager_; ///< エミッター管理システムへの参照

  std::unordered_map<std::string, BoneEmitterLink> links_; ///< リンク名からリンク情報へのマップ
};

} // namespace Tako
