#include "BoneTracker.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

BoneTracker::BoneTracker()
  : model_(nullptr)
  , emitterManager_(nullptr)
{
}

BoneTracker::~BoneTracker()
{
  ClearLinks();
}

void BoneTracker::Initialize(Model* model, EmitterManager* emitterManager)
{
  model_ = model;
  emitterManager_ = emitterManager;

  if (!model_->HasSkeleton()) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("BoneTracker - Model does not have skeleton", DebugUIManager::LogType::Warning);
#endif
  }
}

void BoneTracker::LinkBoneToEmitter(const std::string& linkName,
  const std::string& boneName,
  const std::string& emitterName,
  const Vector3& offset)
{
  if (!model_ || !emitterManager_) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("BoneTracker not initialized", DebugUIManager::LogType::Error);
#endif
    return;
  }

  // エミッターが存在するか確認
  if (!emitterManager_->GetEmitterByName(emitterName)) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("Emitter '" + emitterName + "' not found in EmitterManager", DebugUIManager::LogType::Error);
#endif
    return;
  }

  // ボーンインデックスを検索
  int32_t boneIndex = FindBoneIndex(boneName);
  if (boneIndex < 0) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("Bone '" + boneName + "' not found in model", DebugUIManager::LogType::Error);
#endif
    return;
  }

  // リンク情報を作成
  BoneEmitterLink link;
  link.boneName = boneName;
  link.emitterName = emitterName;
  link.boneIndex = boneIndex;
  link.offset = offset;
  link.isActive = true;

  // リンクを登録
  links_[linkName] = link;

#ifdef _DEBUG
  DebugUIManager::GetInstance()->AddLog("BoneTracker: Linked bone '" + boneName + "' to emitter '" + emitterName + "' (link: '" + linkName + "')", DebugUIManager::LogType::Info);
#endif
}

void BoneTracker::CreateAndLinkSphereEmitter(const std::string& linkName,
  const std::string& boneName,
  float radius, uint32_t count, float frequency,
  const Vector3& offset)
{
  if (!emitterManager_) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("EmitterManager not set", DebugUIManager::LogType::Error);
#endif
    return;
  }

  // エミッター名を生成
  std::string emitterName = linkName + "_emitter";

  // エミッターを作成
  emitterManager_->CreateSphereEmitter(emitterName, Vector3(0.0f, 0.0f, 0.0f),
    radius, count, frequency);

  // ボーンにリンク
  LinkBoneToEmitter(linkName, boneName, emitterName, offset);
}

void BoneTracker::CreateAndLinkBoxEmitter(const std::string& linkName,
  const std::string& boneName,
  const Vector3& size, const Vector3& rotation,
  uint32_t count, float frequency,
  const Vector3& offset)
{
  if (!emitterManager_) {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("EmitterManager not set", DebugUIManager::LogType::Error);
#endif
    return;
  }

  // エミッター名を生成
  std::string emitterName = linkName + "_emitter";

  // エミッターを作成
  emitterManager_->CreateBoxEmitter(emitterName, Vector3(0.0f, 0.0f, 0.0f),
    size, rotation, count, frequency);

  // ボーンにリンク
  LinkBoneToEmitter(linkName, boneName, emitterName, offset);
}

void BoneTracker::Update(const Transform& transform)
{
  if (!model_ || !emitterManager_ || !model_->HasSkeleton()) {
    return;
  }

  const Skeleton& skeleton = model_->GetSkeleton();

  // 全てのリンクを更新
  for (auto& [linkName, link] : links_) {
    if (!link.isActive) {
      continue;
    }

    // ボーンインデックスが有効か確認
    if (link.boneIndex < 0 || link.boneIndex >= static_cast<int32_t>(skeleton.joints.size())) {
      continue;
    }

    // 対象ボーンを取得
    const Joint& targetJoint = skeleton.joints[link.boneIndex];

    // ボーンのワールド座標を計算
    Matrix4x4 worldMatrix = Mat4x4::MakeAffine(
      transform.scale,
      transform.rotate,
      transform.translate
    );
    Matrix4x4 boneWorldMatrix = targetJoint.skeletonSpaceMatrix * worldMatrix;
    Vector3 bonePosition = Mat4x4::Transform(boneWorldMatrix, Vector3(0.0f, 0.0f, 0.0f));

    // オフセットを適用
    if (link.offset.x != 0.0f || link.offset.y != 0.0f || link.offset.z != 0.0f) {
      // ボーンの回転も考慮してオフセットを適用
      Vector3 worldOffset = Mat4x4::Transform(boneWorldMatrix, link.offset) - bonePosition;
      bonePosition = bonePosition + worldOffset;
    }

    // EmitterManagerを使ってエミッター位置を更新
    emitterManager_->SetEmitterPosition(link.emitterName, bonePosition);
  }
}

void BoneTracker::SetLinkActive(const std::string& linkName, bool active)
{
  auto it = links_.find(linkName);
  if (it != links_.end()) {
    it->second.isActive = active;

    // エミッターのアクティブ状態も連動
    if (emitterManager_) {
      emitterManager_->SetEmitterActive(it->second.emitterName, active);
    }
  } else {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("Link '" + linkName + "' not found", DebugUIManager::LogType::Warning);
#endif
  }
}

void BoneTracker::SetLinkOffset(const std::string& linkName, const Vector3& offset)
{
  auto it = links_.find(linkName);
  if (it != links_.end()) {
    it->second.offset = offset;
  } else {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("Link '" + linkName + "' not found", DebugUIManager::LogType::Warning);
#endif
  }
}

void BoneTracker::RemoveLink(const std::string& linkName)
{
  auto it = links_.find(linkName);
  if (it != links_.end()) {
    // エミッターも削除
    if (emitterManager_) {
      emitterManager_->RemoveEmitter(it->second.emitterName);
    }

    links_.erase(it);
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("BoneTracker: Removed link '" + linkName + "'", DebugUIManager::LogType::Info);
#endif
  }
}

void BoneTracker::ClearLinks()
{
  // 全てのリンクされたエミッターを削除
  for (const auto& [linkName, link] : links_) {
    if (emitterManager_) {
      emitterManager_->RemoveEmitter(link.emitterName);
    }
  }

  links_.clear();
#ifdef _DEBUG
  DebugUIManager::GetInstance()->AddLog("BoneTracker: Cleared all links", DebugUIManager::LogType::Info);
#endif
}

bool BoneTracker::HasLink(const std::string& linkName) const
{
  return links_.find(linkName) != links_.end();
}

int32_t BoneTracker::FindBoneIndex(const std::string& boneName)
{
  if (!model_ || !model_->HasSkeleton()) {
    return -1;
  }

  const Skeleton& skeleton = model_->GetSkeleton();
  auto it = skeleton.jointMap.find(boneName);
  if (it != skeleton.jointMap.end()) {
    return it->second;
  }

  return -1;
}

} // namespace Tako