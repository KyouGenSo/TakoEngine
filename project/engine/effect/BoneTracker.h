#pragma once
#include "Model.h"
#include "EmitterManager.h"
#include <string>
#include <unordered_map>
#include <memory>

// ボーンとエミッターの関連付け情報
struct BoneEmitterLink
{
  std::string boneName;       // 追跡するボーン名
  std::string emitterName;    // EmitterManager内のエミッター名
  int32_t boneIndex;          // ボーンインデックス（キャッシュ用）
  Vector3 offset;             // ボーンからのオフセット
  bool isActive;              // アクティブ状態
};

// モデルのボーンにエミッターを追従させるクラス
// 既存のEmitterManagerを活用してボーントラッキングを実現
class BoneTracker
{
public:
  BoneTracker();
  ~BoneTracker();

  // 初期化（対象モデルとEmitterManagerを設定）
  void Initialize(Model* model, EmitterManager* emitterManager);

  // ボーンとエミッターをリンク
  // linkName: リンクの識別名
  // boneName: 追跡するボーン名
  // emitterName: EmitterManager内のエミッター名
  // offset: ボーンからのオフセット位置
  void LinkBoneToEmitter(const std::string& linkName,
    const std::string& boneName,
    const std::string& emitterName,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  // エミッターを作成してボーンにリンク（便利関数）
  // 球体エミッター
  void CreateAndLinkSphereEmitter(const std::string& linkName,
    const std::string& boneName,
    float radius, uint32_t count, float frequency,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  // 箱型エミッター
  void CreateAndLinkBoxEmitter(const std::string& linkName,
    const std::string& boneName,
    const Vector3& size, const Vector3& rotation,
    uint32_t count, float frequency,
    const Vector3& offset = Vector3(0.0f, 0.0f, 0.0f));

  // 更新（ボーン位置を取得してエミッター位置を更新）
  void Update(const Matrix4x4& worldMatrix);

  // リンクの有効/無効切り替え
  void SetLinkActive(const std::string& linkName, bool active);

  // リンクのオフセット変更
  void SetLinkOffset(const std::string& linkName, const Vector3& offset);

  // リンクを削除
  void RemoveLink(const std::string& linkName);

  // 全てのリンクを削除
  void ClearLinks();

  // デバッグ情報取得
  bool HasLink(const std::string& linkName) const;
  size_t GetLinkCount() const { return links_.size(); }

private:
  // ボーンインデックスを検索してキャッシュ
  int32_t FindBoneIndex(const std::string& boneName);

private:
  // 対象モデル
  Model* model_;

  // EmitterManager
  EmitterManager* emitterManager_;

  // ボーンとエミッターのリンク情報
  std::unordered_map<std::string, BoneEmitterLink> links_;
};
