#include "CharacterControllerComponent.h"
#include "Actor.h"
#include "ColliderComponent.h"

namespace Tako {

  CharacterControllerComponent::CharacterControllerComponent() = default;

  CharacterControllerComponent::~CharacterControllerComponent()
  {
    EndPlay();
  }

  void CharacterControllerComponent::Awake()
  {
    CreateCharacter();
  }

  void CharacterControllerComponent::EndPlay()
  {
    DestroyCharacter();
  }

  void CharacterControllerComponent::OnActiveChanged(bool active)
  {
    if (character_ != PhysicsWorld::kInvalidCharacter) {
      PhysicsWorld::GetInstance()->SetCharacterEnabled(character_, active);
    }
  }

  void CharacterControllerComponent::PostEditChange()
  {
    // 形と設定は作るときに決まるので、作り直す
    if (character_ != PhysicsWorld::kInvalidCharacter) {
      DestroyCharacter();
      CreateCharacter();
    }
  }

#ifdef TAKO_EDITOR
  bool CharacterControllerComponent::DrawCustomImGui()
  {
    return ColliderComponent::DrawLayerCombo(layer_);
  }
#endif

  void CharacterControllerComponent::SetVelocity(const Vector3& velocity)
  {
    PhysicsWorld::GetInstance()->SetCharacterVelocity(character_, velocity);
  }

  Vector3 CharacterControllerComponent::GetVelocity() const
  {
    return PhysicsWorld::GetInstance()->GetCharacterVelocity(character_);
  }

  bool CharacterControllerComponent::IsGrounded() const
  {
    return PhysicsWorld::GetInstance()->IsCharacterGrounded(character_);
  }

  void CharacterControllerComponent::CreateCharacter()
  {
    PhysicsWorld::CharacterSettings settings;
    settings.actor         = GetOwner();
    settings.owner         = this;
    settings.layer         = layer_;
    settings.radius        = radius_;
    settings.height        = height_;
    settings.offset        = offset_;
    settings.maxSlopeAngle = maxSlopeAngle_;
    settings.stepHeight    = stepHeight_;
    settings.gravityScale  = gravityScale_;

    PhysicsWorld* physics = PhysicsWorld::GetInstance();
    character_ = physics->CreateCharacter(settings);
    if (!GetOwner()->IsActiveInHierarchy()) {
      physics->SetCharacterEnabled(character_, false);
    }
  }

  void CharacterControllerComponent::DestroyCharacter()
  {
    if (character_ != PhysicsWorld::kInvalidCharacter) {
      PhysicsWorld::GetInstance()->DestroyCharacter(character_);
      character_ = PhysicsWorld::kInvalidCharacter;
    }
  }

} // namespace Tako
