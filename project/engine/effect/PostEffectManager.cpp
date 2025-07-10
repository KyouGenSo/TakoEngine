#include "PostEffectManager.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "Logger.h"
#include "StringUtility.h"
#include "Object3dbasic.h"
#include "Camera.h"
#include "IPostEffect.h"
#include "NoEffect.h"
#include "GrayScale.h"
#include "Vignette.h"
#include "RadialBlur.h"
#include "RGBSplit.h"
#include "BWFilter.h"
#include "LuminanceBasedOutline.h"
#include "DepthBasedOutline.h"
#include "Fog.h"
#include "Bloom.h"
#include "Dissolve.h"
#include "WhiteNoise.h"

#include <algorithm>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

PostEffectManager* PostEffectManager::instance_ = nullptr;

PostEffectManager* PostEffectManager::GetInstance()
{
  if (instance_ == nullptr)
  {
    instance_ = new PostEffectManager();
  }
  return instance_;
}

void PostEffectManager::Initialize(DX12Basic* dx12)
{
  m_dx12_ = dx12;

  CreateRenderTextures();

  // デフォルトエフェクトの登録
  RegisterEffect("NoEffect", std::make_unique<NoEffect>());
  RegisterEffect("GrayScale", std::make_unique<GrayScale>());
  RegisterEffect("Vignette", std::make_unique<Vignette>());
  RegisterEffect("RadialBlur", std::make_unique<RadialBlur>());
  RegisterEffect("RGBSplit", std::make_unique<RGBSplit>());
  RegisterEffect("BWFilter", std::make_unique<BWFilter>());
  RegisterEffect("LuminanceBasedOutline", std::make_unique<LuminanceBasedOutline>());
  RegisterEffect("DepthBasedOutline", std::make_unique<DepthBasedOutline>());
  RegisterEffect("Fog", std::make_unique<Fog>());
  RegisterEffect("Bloom", std::make_unique<Bloom>());
  RegisterEffect("Dissolve", std::make_unique<Dissolve>());
  RegisterEffect("WhiteNoise", std::make_unique<WhiteNoise>());

  // 深度バッファテクスチャのSRV作成
  depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
  SrvManager::GetInstance()->CreateSRVForTexture2D(
    depthSrvIndex_,
    m_dx12_->GetDepthStencilResource(),
    DXGI_FORMAT_R32_FLOAT,
    1
  );

  // DissolveマスクテクスチャのデフォルトSRV作成
  dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSRVIndex("noise0.png");
}

void PostEffectManager::Finalize()
{
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

void PostEffectManager::AddEffectToChain(const std::string& name)
{
  if (effectRegistry_.find(name) != effectRegistry_.end()) {
    effectChain_.push_back(name);
  }
}

void PostEffectManager::RemoveEffectFromChain(const std::string& name)
{
  effectChain_.erase(
    std::remove(effectChain_.begin(), effectChain_.end(), name),
    effectChain_.end()
  );
}

void PostEffectManager::ClearEffectChain()
{
  effectChain_.clear();
}

void PostEffectManager::BeginDrawEffectTarget()
{
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  // エフェクト適用対象RTに描画
  m_dx12_->GetCommandList()->OMSetRenderTargets(
    1,
    &effectTargetRT_.rtvHandle,
    false,
    &dsvHandle);

  float clearColor[] = {
      kEffectTargetClearColor_.x,
      kEffectTargetClearColor_.y,
      kEffectTargetClearColor_.z,
      kEffectTargetClearColor_.w
  };

  // エフェクト適用対象RTをクリア
  m_dx12_->GetCommandList()->ClearRenderTargetView(effectTargetRT_.rtvHandle, clearColor, 0, nullptr);
}

void PostEffectManager::BegineDrawNonEffectTarget()
{
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  // 非適用対象RTに描画
  m_dx12_->GetCommandList()->OMSetRenderTargets(
    1,
    &nonEffectTargetRT_.rtvHandle,
    false,
    &dsvHandle);
}

void PostEffectManager::Draw()
{
  ApplyEffectChain();
}

void PostEffectManager::DrawFinalResult()
{
  // 非適用対象RTをシェーダーリソースに遷移
  SetBarrier(
    nonEffectTargetRT_.resource.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );

  // スワップチェインに描画
  m_dx12_->SetSwapChain();

  // NoEffectを使って単純コピー
  if (effectRegistry_.find("NoEffect") != effectRegistry_.end()) {
    auto& baseEffect = effectRegistry_["NoEffect"];

    // NoEffectにダウンキャスト
    NoEffect* noEffect = dynamic_cast<NoEffect*>(baseEffect.get());
    if (noEffect != nullptr) {
      noEffect->ApplyToBackBuffer(nonEffectTargetRT_.srvIndex);
    }
  }

  // リソースを元に戻す
  SetBarrier(
    nonEffectTargetRT_.resource.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET
  );
}

void PostEffectManager::DrawImgui()
{
#ifdef _DEBUG
  ImGui::Begin("PostEffect Manager");

  if (ImGui::BeginTabBar("PostEffectTab"))
  {
    if (ImGui::BeginTabItem("PostEffect"))
    {
      ImGui::Text("Post Effects Configuration");
      ImGui::Separator();

      // 2カラムレイアウト
      ImGui::Columns(2, "EffectColumns", true);

      // === 左パネル: 利用可能なエフェクト ===
      ImGui::Text("Available Effects");
      ImGui::Separator();

      // 利用可能エフェクトリスト
      ImGui::BeginChild("AvailableList", ImVec2(0, 200), true);
      for (const auto& effectName : availableEffects_) {
        bool isSelected = (selectedAvailableEffect_ == effectName);

        if (ImGui::Selectable(effectName.c_str(), isSelected)) {
          selectedAvailableEffect_ = effectName;
        }
      }
      ImGui::EndChild();

      // 適用ボタン
      bool canApply = !selectedAvailableEffect_.empty() &&
        !IsEffectInChain(selectedAvailableEffect_);

      if (!canApply) {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button("Apply Effect", ImVec2(-1, 0))) {
        if (!selectedAvailableEffect_.empty()) {
          AddEffectToChain(selectedAvailableEffect_);
        }
      }

      if (!canApply) {
        ImGui::EndDisabled();
      }

      // 状態表示
      if (!selectedAvailableEffect_.empty()) {
        if (IsEffectInChain(selectedAvailableEffect_)) {
          ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Already applied");
        } else {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
            "Ready to apply");
        }
      }

      ImGui::NextColumn();

      // === 右パネル: アクティブなエフェクト ===
      ImGui::Text("Active Effects");
      ImGui::Separator();

      // アクティブエフェクトリスト
      ImGui::BeginChild("ActiveList", ImVec2(0, 200), true);

      size_t chainSize = GetEffectChainSize();
      if (chainSize == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No effects active");
      } else {
        for (size_t i = 0; i < chainSize; ++i) {
          std::string effectName = GetEffectAtPosition(static_cast<int>(i));
          if (effectName.empty()) continue;

          bool isSelected = (selectedActiveEffect_ == effectName);

          // エフェクト名に順序番号を付けて表示
          std::string displayName = std::to_string(i + 1) + ". " + effectName;

          if (ImGui::Selectable(displayName.c_str(), isSelected)) {
            selectedActiveEffect_ = effectName;
          }
        }
      }

      ImGui::EndChild();

      // 操作ボタン
      bool hasSelection = !selectedActiveEffect_.empty() &&
        IsEffectInChain(selectedActiveEffect_);

      // Remove ボタン
      if (!hasSelection) {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button("Remove Effect", ImVec2(-1, 0))) {
        if (!selectedActiveEffect_.empty()) {
          RemoveEffectFromChain(selectedActiveEffect_);
          selectedActiveEffect_ = ""; // 選択解除
        }
      }

      if (!hasSelection) {
        ImGui::EndDisabled();
      }

      ImGui::Spacing();

      // 順序変更ボタン
      ImGui::BeginGroup();

      bool canMoveUp = hasSelection && GetEffectPosition(selectedActiveEffect_) > 0;
      bool canMoveDown = hasSelection &&
        GetEffectPosition(selectedActiveEffect_) < static_cast<int>(GetEffectChainSize()) - 1;

      if (!canMoveUp) {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button("Move Up", ImVec2(80, 0))) {
        MoveEffectUp(selectedActiveEffect_);
      }

      if (!canMoveUp) {
        ImGui::EndDisabled();
      }

      ImGui::SameLine();

      if (!canMoveDown) {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button("Move Down", ImVec2(-1, 0))) {
        MoveEffectDown(selectedActiveEffect_);
      }

      if (!canMoveDown) {
        ImGui::EndDisabled();
      }

      ImGui::EndGroup();

      ImGui::Spacing();

      // 全体操作ボタン
      ImGui::BeginGroup();

      if (ImGui::Button("Clear All", ImVec2(80, 0))) {
        ClearEffectChain();
        selectedActiveEffect_ = ""; // 選択解除
      }

      ImGui::SameLine();

      bool hasEffects = GetEffectChainSize() > 0;
      if (!hasEffects) {
        ImGui::BeginDisabled();
      }

      if (ImGui::Button("Reverse Order", ImVec2(-1, 0))) {
        std::reverse(effectChain_.begin(), effectChain_.end());
      }

      if (!hasEffects) {
        ImGui::EndDisabled();
      }

      ImGui::EndGroup();

      // 状態表示
      if (!selectedActiveEffect_.empty()) {
        int position = GetEffectPosition(selectedActiveEffect_);
        if (position >= 0) {
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f),
            "Position: %d", position + 1);
        }
      }

      ImGui::Columns(1);

      // === 全体情報表示 ===
      ImGui::Separator();
      ImGui::Text("Status:");

      chainSize = GetEffectChainSize();
      if (chainSize == 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
          "Using NoEffect (no post-processing)");
      } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
          "%zu effect(s) active", chainSize);

        // エフェクト順序表示
        ImGui::Text("Effect order: ");
        ImGui::SameLine();
        for (size_t i = 0; i < chainSize; ++i) {
          if (i > 0) {
            ImGui::SameLine();
            ImGui::Text("→");
            ImGui::SameLine();
          }
          ImGui::Text("%s", GetEffectAtPosition(static_cast<int>(i)).c_str());
        }
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("PostEffectParam"))
    {
      DrawEffectParametersTab();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
#endif
}

void PostEffectManager::RecreateRenderTexture()
{
  // 既存のリソースを解放
  effectTargetRT_.resource.Reset();
  nonEffectTargetRT_.resource.Reset();

  SrvManager::GetInstance()->Free(effectTargetRT_.srvIndex);
  SrvManager::GetInstance()->Free(nonEffectTargetRT_.srvIndex);

  for (auto& rt : intermediateRTs_) {
    rt.resource.Reset();
    SrvManager::GetInstance()->Free(rt.srvIndex);
  }

  // 新しいサイズで再作成
  CreateRenderTextures();

  // 深度バッファのSRVも更新
  SrvManager::GetInstance()->CreateSRVForTexture2D(
    depthSrvIndex_,
    m_dx12_->GetDepthStencilResource(),
    DXGI_FORMAT_R32_FLOAT,
    1
  );
}

bool PostEffectManager::SetEffectParam(const std::string& effectName, const EffectParam& param)
{
  // エフェクトが存在するかチェック
  auto it = effectRegistry_.find(effectName);
  if (it == effectRegistry_.end()) {
    // ログ出力（デバッグ用）
#ifdef _DEBUG
    Logger::Log("Effect not found: " + effectName);
#endif
    return false;
  }

  // エフェクトにパラメーターを設定
  bool success = it->second->SetGenericParam(param);

#ifdef _DEBUG
  if (!success) {
    Logger::Log("Parameter type mismatch for effect: " + effectName);
  }
#endif

  return success;
}

void PostEffectManager::SetDissolveBaseTex(const std::string& textureName)
{
  auto it = effectRegistry_.find("Dissolve");
  if (it != effectRegistry_.end()) {
    auto dissolveEffect = dynamic_cast<Dissolve*>(it->second.get());
    if (dissolveEffect) {
      dissolveEffect->SetBaseTextureSrvIndex(TextureManager::GetInstance()->GetSRVIndex(textureName));
    }
  }
}

bool PostEffectManager::MoveEffectUp(const std::string& effectName)
{
  auto it = std::find(effectChain_.begin(), effectChain_.end(), effectName);
  if (it == effectChain_.end() || it == effectChain_.begin()) {
    return false; // エフェクトが見つからない、または既に最上位
  }

  // 一つ上の要素と交換
  std::iter_swap(it, it - 1);
  return true;
}

bool PostEffectManager::MoveEffectDown(const std::string& effectName)
{
  auto it = std::find(effectChain_.begin(), effectChain_.end(), effectName);
  if (it == effectChain_.end() || it == effectChain_.end() - 1) {
    return false; // エフェクトが見つからない、または既に最下位
  }

  // 一つ下の要素と交換
  std::iter_swap(it, it + 1);
  return true;
}

bool PostEffectManager::MoveEffectToPosition(const std::string& effectName, int newPosition)
{
  // 範囲チェック
  if (newPosition < 0 || newPosition >= static_cast<int>(effectChain_.size())) {
    return false;
  }

  // 現在の位置を取得
  auto it = std::find(effectChain_.begin(), effectChain_.end(), effectName);
  if (it == effectChain_.end()) {
    return false; // エフェクトが見つからない
  }

  int currentPosition = static_cast<int>(std::distance(effectChain_.begin(), it));
  if (currentPosition == newPosition) {
    return true; // 既に目的の位置にある
  }

  // エフェクトを一時的に保存して削除
  std::string tempEffect = *it;
  effectChain_.erase(it);

  // 新しい位置に挿入
  if (newPosition > currentPosition) {
    // 削除により位置がずれるため調整
    newPosition--;
  }
  effectChain_.insert(effectChain_.begin() + newPosition, tempEffect);

  return true;
}

bool PostEffectManager::SwapEffects(const std::string& effectName1, const std::string& effectName2)
{
  auto it1 = std::find(effectChain_.begin(), effectChain_.end(), effectName1);
  auto it2 = std::find(effectChain_.begin(), effectChain_.end(), effectName2);

  if (it1 == effectChain_.end() || it2 == effectChain_.end()) {
    return false; // どちらかのエフェクトが見つからない
  }

  // 要素を交換
  std::iter_swap(it1, it2);
  return true;
}

bool PostEffectManager::SwapEffectsByIndex(int index1, int index2)
{
  // 範囲チェック
  if (index1 < 0 || index1 >= static_cast<int>(effectChain_.size()) ||
    index2 < 0 || index2 >= static_cast<int>(effectChain_.size())) {
    return false;
  }

  if (index1 == index2) {
    return true; // 同じインデックス
  }

  // 要素を交換
  std::swap(effectChain_[index1], effectChain_[index2]);
  return true;
}

int PostEffectManager::GetEffectPosition(const std::string& effectName) const
{
  auto it = std::find(effectChain_.begin(), effectChain_.end(), effectName);
  if (it == effectChain_.end()) {
    return -1; // エフェクトが見つからない
  }

  return static_cast<int>(std::distance(effectChain_.begin(), it));
}

bool PostEffectManager::IsEffectInChain(const std::string& effectName) const
{
  return std::find(effectChain_.begin(), effectChain_.end(), effectName) != effectChain_.end();
}

size_t PostEffectManager::GetEffectChainSize() const
{
  return effectChain_.size();
}

std::string PostEffectManager::GetEffectAtPosition(int position) const
{
  if (position < 0 || position >= static_cast<int>(effectChain_.size())) {
    return ""; // 無効な位置
  }

  return effectChain_[position];
}

std::vector<std::string> PostEffectManager::GetEffectChain() const
{
  return effectChain_; // コピーを返す
}

//------------------------------- プライベート関数 -------------------------------//

void PostEffectManager::CreateRenderTextures() {
  auto createRT = [this](RenderTexture& rt, int rtvIndex, const Vector4& clearColor) {
    // リソース作成
    m_dx12_->CreateRenderTextureResource(
      rt.resource,
      WinApp::clientWidth,
      WinApp::clientHeight,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      clearColor
    );

    // RTV作成
    rt.rtvHandle = m_dx12_->GetRenderTextureRTVHandle(rtvIndex);
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    m_dx12_->GetDevice()->CreateRenderTargetView(
      rt.resource.Get(), &rtvDesc, rt.rtvHandle
    );

    // SRV作成
    rt.srvIndex = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      rt.srvIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1
    );
    };

  // エフェクト適用対象用RT
  createRT(effectTargetRT_, 2, kEffectTargetClearColor_);
  effectTargetRT_.resource->SetName(L"EffectTargetRT");

  // 非適用対象用RT
  createRT(nonEffectTargetRT_, 3, nonEffectTargetClearColor_);
  nonEffectTargetRT_.resource->SetName(L"NonEffectTargetRT");

  // 中間バッファ
  intermediateRTs_.resize(2);
  for (size_t i = 0; i < intermediateRTs_.size(); ++i) {
    createRT(intermediateRTs_[i], 4 + static_cast<int>(i), kEffectTargetClearColor_);
    intermediateRTs_[i].resource->SetName(L"IntermediateRT");
  }
}

void PostEffectManager::RegisterEffect(const std::string& name, std::unique_ptr<IPostEffect> effect)
{
  if (effectRegistry_.find(name) != effectRegistry_.end()) {
    return;
  }
  effectRegistry_[name] = std::move(effect);
  effectRegistry_[name]->Initialize(m_dx12_, name);

  // 利用可能なエフェクトリストに追加（NoEffect以外）
  if (name != "NoEffect") {
    availableEffects_.push_back(name);
  }

}

void PostEffectManager::ApplyEffectChain()
{
  // エフェクトチェーンにNoEffectを自動追加せず、空の場合のみNoEffectを使用
  std::vector<std::string> actualChain;

  if (effectChain_.empty()) {
    // エフェクトチェーンが空の場合はNoEffectのみ
    actualChain.push_back("NoEffect");
  } else {
    // エフェクトチェーンがある場合は、そのまま使用（NoEffectは追加しない）
    actualChain = effectChain_;
  }

  // 最初の入力はエフェクト適用対象RT
  SetBarrier(
    effectTargetRT_.resource.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );

  // 深度バッファチェック
  bool needsDepthBuffer = false;
  for (const auto& effectName : actualChain) {
    if (effectRegistry_[effectName]->RequiresDepthBuffer()) {
      needsDepthBuffer = true;
      break;
    }
  }

  if (needsDepthBuffer) {
    m_dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      m_dx12_->GetDepthStencilResource()
    );
  }

  // 中間バッファの状態追跡
  std::vector<bool> intermediateRTStates(intermediateRTs_.size(), false);

  // エフェクト適用処理
  uint32_t srcSrvIndex = effectTargetRT_.srvIndex;
  D3D12_CPU_DESCRIPTOR_HANDLE dstRtvHandle;

  for (size_t i = 0; i < actualChain.size(); ++i) {
    const auto& effectName = actualChain[i];
    auto& effect = effectRegistry_[effectName];

    if (!effect) continue;

    bool isLastEffect = (i == actualChain.size() - 1);

    if (isLastEffect) {
      dstRtvHandle = nonEffectTargetRT_.rtvHandle;
    } else {
      int bufferIndex = i % 2;
      auto& intermediateRT = intermediateRTs_[bufferIndex];

      if (intermediateRTStates[bufferIndex]) {
        SetBarrier(
          intermediateRT.resource.Get(),
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
          D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        intermediateRTStates[bufferIndex] = false;
      }

      dstRtvHandle = intermediateRT.rtvHandle;
    }

    if (effectName == "DepthBasedOutline") {
      auto it = effectRegistry_.find(effectName);
      if (it != effectRegistry_.end()) {
        // DepthBasedOutlineの初期化
        auto depthEffect = dynamic_cast<DepthBasedOutline*>(it->second.get());
        if (depthEffect) {
          depthEffect->SetInvProjectionMatrix(Mat4x4::Inverse(camera_->GetProjectionMatrix()));
        }
      }
    }

    if (effectName == "Dissolve")
    {
      effect->Apply(srcSrvIndex, dstRtvHandle, dissolveMaskSrvIndex_, nonEffectTargetClearColor_);
    } else
    {
      effect->Apply(srcSrvIndex, dstRtvHandle, depthSrvIndex_, nonEffectTargetClearColor_);
    }


    if (!isLastEffect) {
      int bufferIndex = i % 2;
      auto& usedRT = intermediateRTs_[bufferIndex];

      SetBarrier(
        usedRT.resource.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
      );
      intermediateRTStates[bufferIndex] = true;

      srcSrvIndex = usedRT.srvIndex;
    }
  }

  // リソース復元
  if (needsDepthBuffer) {
    m_dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      m_dx12_->GetDepthStencilResource()
    );
  }

  SetBarrier(
    effectTargetRT_.resource.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET
  );

  for (size_t i = 0; i < intermediateRTs_.size(); ++i) {
    if (intermediateRTStates[i]) {
      auto& rt = intermediateRTs_[i];
      SetBarrier(
        rt.resource.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET
      );
    }
  }
}

void PostEffectManager::DrawEffectParametersTab()
{
#ifdef _DEBUG
  size_t chainSize = GetEffectChainSize();

  if (chainSize == 0) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
      "No active effects to configure");
    ImGui::TextWrapped("Switch to the 'PostEffect' tab to enable effects first.");
  } else {
    ImGui::Text("Effect Parameters:");
    ImGui::Separator();

    for (size_t i = 0; i < chainSize; ++i) {
      std::string effectName = GetEffectAtPosition(static_cast<int>(i));
      if (!effectName.empty()) {
        std::string headerName = "[" + std::to_string(i + 1) + "] " + effectName;

        if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
          if (effectRegistry_.find(effectName) != effectRegistry_.end()) {
            effectRegistry_[effectName]->DrawImgui();
          }
        }
      }
    }
  }
#endif
}

void PostEffectManager::SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = effectTargetRT_.resource.Get();

  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.StateAfter = stateAfter;

  m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void PostEffectManager::SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource;
  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.StateAfter = stateAfter;

  m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
}