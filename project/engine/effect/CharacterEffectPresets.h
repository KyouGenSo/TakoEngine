#pragma once
#include "BoneTracker.h"
#include "EmitterManager.h"
#include "Model.h"

/// <summary>
/// キャラクターエフェクトのプリセット集
/// 既存のEmitterManagerとBoneTrackerを使用してエフェクトを作成
/// </summary>
namespace CharacterEffectPresets
{
  /// <summary>
  /// 炎の拳エフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_FIRE_FIST = "fire_fist_effect";

  /// <summary>
  /// 氷のオーラエフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_ICE_AURA = "ice_aura_effect";

  /// <summary>
  /// 電撃エフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_LIGHTNING = "lightning_effect";

  /// <summary>
  /// 回復エフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_HEALING = "healing_effect";

  /// <summary>
  /// 毒エフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_POISON = "poison_effect";

  /// <summary>
  /// 武器の軌跡エフェクトグループ名
  /// </summary>
  constexpr const char* GROUP_WEAPON_TRAIL = "weapon_trail_effect";

  /// <summary>
  /// 炎の拳エフェクトを作成（両手から炎パーティクルを放出）
  /// </summary>
  /// <param name="emitterManager">エミッターマネージャーへのポインタ</param>
  /// <param name="boneTracker">ボーントラッカーへのポインタ</param>
  inline void CreateFireFistEffect(EmitterManager* emitterManager, BoneTracker* boneTracker)
  {
    // グループを作成
    emitterManager->CreateGroup(GROUP_FIRE_FIST);

    // 左手の炎エミッター
    emitterManager->CreateSphereEmitter("fire_left", Vector3(0.0f, 0.0f, 0.0f), 0.1f, 30, 0.02f);
    emitterManager->SetEmitterScaleRange("fire_left", Vector2(0.05f, 0.15f), Vector2(0.05f, 0.15f));
    emitterManager->SetEmitterVelocityRange("fire_left", Vector2(-0.2f, 0.2f), Vector2(0.5f, 1.5f), Vector2(-0.2f, 0.2f));
    emitterManager->SetEmitterLifeTimeRange("fire_left", Vector2(0.5f, 1.0f));
    emitterManager->SetEmitterColors("fire_left",
      Vector4(1.0f, 0.4f, 0.1f, 1.0f),  // オレンジ色
      Vector4(1.0f, 0.1f, 0.0f, 0.0f)   // 赤から透明へ
    );
    emitterManager->AddToGroup(GROUP_FIRE_FIST, "fire_left");

    // 右手の炎エミッター
    emitterManager->CreateSphereEmitter("fire_right", Vector3(0.0f, 0.0f, 0.0f), 0.1f, 30, 0.02f);
    emitterManager->SetEmitterScaleRange("fire_right", Vector2(0.05f, 0.15f), Vector2(0.05f, 0.15f));
    emitterManager->SetEmitterVelocityRange("fire_right", Vector2(-0.2f, 0.2f), Vector2(0.5f, 1.5f), Vector2(-0.2f, 0.2f));
    emitterManager->SetEmitterLifeTimeRange("fire_right", Vector2(0.5f, 1.0f));
    emitterManager->SetEmitterColors("fire_right",
      Vector4(1.0f, 0.4f, 0.1f, 1.0f),
      Vector4(1.0f, 0.1f, 0.0f, 0.0f)
    );
    emitterManager->AddToGroup(GROUP_FIRE_FIST, "fire_right");

    // ボーンにリンク
    boneTracker->LinkBoneToEmitter("fire_left_link", "Hand_L", "fire_left");
    boneTracker->LinkBoneToEmitter("fire_right_link", "Hand_R", "fire_right");
  }

  /// <summary>
  /// 氷のオーラエフェクトを作成（全身の主要部位から氷パーティクルを放出）
  /// </summary>
  /// <param name="emitterManager">エミッターマネージャーへのポインタ</param>
  /// <param name="boneTracker">ボーントラッカーへのポインタ</param>
  inline void CreateIceAuraEffect(EmitterManager* emitterManager, BoneTracker* boneTracker)
  {
    // グループを作成
    emitterManager->CreateGroup(GROUP_ICE_AURA);

    // 頭部
    emitterManager->CreateSphereEmitter("ice_head", Vector3(0.0f, 0.0f, 0.0f), 0.5f, 10, 0.1f);
    emitterManager->SetEmitterScaleRange("ice_head", Vector2(0.02f, 0.05f), Vector2(0.02f, 0.05f));
    emitterManager->SetEmitterVelocityRange("ice_head", Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f));
    emitterManager->SetEmitterLifeTimeRange("ice_head", Vector2(2.0f, 3.0f));
    emitterManager->SetEmitterColors("ice_head",
      Vector4(0.7f, 0.9f, 1.0f, 0.8f),  // 水色
      Vector4(0.4f, 0.6f, 1.0f, 0.0f)   // 青から透明へ
    );
    emitterManager->AddToGroup(GROUP_ICE_AURA, "ice_head");
    boneTracker->LinkBoneToEmitter("ice_head_link", "Head", "ice_head", Vector3(0.0f, 0.1f, 0.0f));

    // 胸部
    emitterManager->CreateSphereEmitter("ice_chest", Vector3(0.0f, 0.0f, 0.0f), 0.5f, 10, 0.1f);
    emitterManager->SetEmitterScaleRange("ice_chest", Vector2(0.02f, 0.05f), Vector2(0.02f, 0.05f));
    emitterManager->SetEmitterVelocityRange("ice_chest", Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f));
    emitterManager->SetEmitterLifeTimeRange("ice_chest", Vector2(2.0f, 3.0f));
    emitterManager->SetEmitterColors("ice_chest",
      Vector4(0.7f, 0.9f, 1.0f, 0.8f),
      Vector4(0.4f, 0.6f, 1.0f, 0.0f)
    );
    emitterManager->AddToGroup(GROUP_ICE_AURA, "ice_chest");
    boneTracker->LinkBoneToEmitter("ice_chest_link", "Spine2", "ice_chest");

    // 両手（小さめ）
    const char* handNames[] = { "ice_left_hand", "ice_right_hand" };
    const char* handBones[] = { "Hand_L", "Hand_R" };

    for (int i = 0; i < 2; ++i) {
      emitterManager->CreateSphereEmitter(handNames[i], Vector3(0.0f, 0.0f, 0.0f), 0.2f, 5, 0.1f);
      emitterManager->SetEmitterScaleRange(handNames[i], Vector2(0.02f, 0.05f), Vector2(0.02f, 0.05f));
      emitterManager->SetEmitterVelocityRange(handNames[i], Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f), Vector2(-0.1f, 0.1f));
      emitterManager->SetEmitterLifeTimeRange(handNames[i], Vector2(2.0f, 3.0f));
      emitterManager->SetEmitterColors(handNames[i],
        Vector4(0.7f, 0.9f, 1.0f, 0.8f),
        Vector4(0.4f, 0.6f, 1.0f, 0.0f)
      );
      emitterManager->AddToGroup(GROUP_ICE_AURA, handNames[i]);

      std::string linkName = std::string(handNames[i]) + "_link";
      boneTracker->LinkBoneToEmitter(linkName, handBones[i], handNames[i]);
    }
  }

  /// <summary>
  /// 電撃エフェクトを作成（頭部と両手から電撃パーティクルを放出）
  /// </summary>
  /// <param name="emitterManager">エミッターマネージャーへのポインタ</param>
  /// <param name="boneTracker">ボーントラッカーへのポインタ</param>
  inline void CreateLightningEffect(EmitterManager* emitterManager, BoneTracker* boneTracker)
  {
    // グループを作成
    emitterManager->CreateGroup(GROUP_LIGHTNING);

    // 頭部から電撃
    emitterManager->CreateSphereEmitter("lightning_head", Vector3(0.0f, 0.0f, 0.0f), 0.05f, 50, 0.01f);
    emitterManager->SetEmitterScaleRange("lightning_head", Vector2(0.01f, 0.03f), Vector2(0.1f, 0.3f));
    emitterManager->SetEmitterVelocityRange("lightning_head", Vector2(-2.0f, 2.0f), Vector2(-2.0f, 2.0f), Vector2(-2.0f, 2.0f));
    emitterManager->SetEmitterLifeTimeRange("lightning_head", Vector2(0.1f, 0.3f));
    emitterManager->SetEmitterColors("lightning_head",
      Vector4(0.8f, 0.8f, 1.0f, 1.0f),  // 青白い
      Vector4(0.4f, 0.4f, 1.0f, 0.0f)
    );
    emitterManager->AddToGroup(GROUP_LIGHTNING, "lightning_head");
    boneTracker->LinkBoneToEmitter("lightning_head_link", "Head", "lightning_head", Vector3(0.0f, 0.2f, 0.0f));

    // 両手から電撃
    const char* handNames[] = { "lightning_left_hand", "lightning_right_hand" };
    const char* handBones[] = { "Hand_L", "Hand_R" };

    for (int i = 0; i < 2; ++i) {
      emitterManager->CreateSphereEmitter(handNames[i], Vector3(0.0f, 0.0f, 0.0f), 0.1f, 50, 0.01f);
      emitterManager->SetEmitterScaleRange(handNames[i], Vector2(0.01f, 0.03f), Vector2(0.1f, 0.3f));
      emitterManager->SetEmitterVelocityRange(handNames[i], Vector2(-2.0f, 2.0f), Vector2(-2.0f, 2.0f), Vector2(-2.0f, 2.0f));
      emitterManager->SetEmitterLifeTimeRange(handNames[i], Vector2(0.1f, 0.3f));
      emitterManager->SetEmitterColors(handNames[i],
        Vector4(0.8f, 0.8f, 1.0f, 1.0f),
        Vector4(0.4f, 0.4f, 1.0f, 0.0f)
      );
      emitterManager->AddToGroup(GROUP_LIGHTNING, handNames[i]);

      std::string linkName = std::string(handNames[i]) + "_link";
      boneTracker->LinkBoneToEmitter(linkName, handBones[i], handNames[i]);
    }
  }

  /// <summary>
  /// 武器の軌跡エフェクトを作成
  /// </summary>
  /// <param name="emitterManager">エミッターマネージャーへのポインタ</param>
  /// <param name="boneTracker">ボーントラッカーへのポインタ</param>
  inline void CreateWeaponTrailEffect(EmitterManager* emitterManager, BoneTracker* boneTracker)
  {
    // グループを作成
    emitterManager->CreateGroup(GROUP_WEAPON_TRAIL);

    // 武器の軌跡エミッター
    emitterManager->CreateSphereEmitter("weapon_trail", Vector3(0.0f, 0.0f, 0.0f), 0.02f, 1, 0.001f);
    emitterManager->SetEmitterScaleRange("weapon_trail", Vector2(0.05f, 0.01f), Vector2(0.05f, 0.01f));
    emitterManager->SetEmitterVelocityRange("weapon_trail", Vector2(0.0f, 0.0f), Vector2(0.0f, 0.0f), Vector2(0.0f, 0.0f));
    emitterManager->SetEmitterLifeTimeRange("weapon_trail", Vector2(0.3f, 0.5f));
    emitterManager->SetEmitterColors("weapon_trail",
      Vector4(1.0f, 1.0f, 1.0f, 1.0f),
      Vector4(0.5f, 0.5f, 1.0f, 0.0f)
    );
    emitterManager->AddToGroup(GROUP_WEAPON_TRAIL, "weapon_trail");

    // 右手の武器位置にリンク（オフセットで武器の先端を指定）
    boneTracker->LinkBoneToEmitter("weapon_trail_link", "Hand_R", "weapon_trail", Vector3(0.0f, 0.0f, -0.5f));

    // 初期は無効化
    emitterManager->SetEmitterActive("weapon_trail", false);
  }

  /// <summary>
  /// エフェクトを削除（グループと関連するボーンリンクを削除）
  /// </summary>
  /// <param name="emitterManager">エミッターマネージャーへのポインタ</param>
  /// <param name="boneTracker">ボーントラッカーへのポインタ</param>
  /// <param name="groupName">削除するエフェクトグループ名</param>
  inline void RemoveEffect(EmitterManager* emitterManager, BoneTracker* boneTracker, const std::string& groupName)
  {
    // グループを削除（関連するエミッターも削除される）
    emitterManager->RemoveGroup(groupName);

    // ボーンリンクも削除
    boneTracker->ClearLinks();
  }
}
