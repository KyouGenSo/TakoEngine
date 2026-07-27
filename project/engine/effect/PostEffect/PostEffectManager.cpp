#include "PostEffectManager.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
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
#include "HalfTone.h"
#include "GaussianBlur.h"
#include "EaseFunc.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  std::unique_ptr<PostEffectManager> PostEffectManager::instance_ = nullptr;

  PostEffectManager* PostEffectManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<PostEffectManager>(Token{});
    }
    return instance_.get();
  }

  void PostEffectManager::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

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
    RegisterEffect("HalfTone", std::make_unique<HalfTone>());
    RegisterEffect("GaussianBlur", std::make_unique<GaussianBlur>());

    // 深度バッファテクスチャの SRV 作成
    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      depthSrvIndex_,
      dx12_->GetDepthStencilResource(),
      DXGI_FORMAT_R32_FLOAT,
      1
    );

    // Dissolve マスクテクスチャのデフォルト SRV 作成
    dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("noise0.png");
  }

  void PostEffectManager::Finalize()
  {
    // RT と深度 SRV を返却してからインスタンスを破棄する
    if (instance_) {
      instance_->effectTargetRT_.Release();
      instance_->nonEffectTargetRT_.Release();
      for (auto& rt : instance_->intermediateRTs_) {
        rt.Release();
      }
      SrvManager::GetInstance()->Free(instance_->depthSrvIndex_);
      instance_->depthSrvIndex_ = 0;
    }

    instance_.reset();
  }

  void PostEffectManager::AddEffectToChain(const std::string& name)
  {
    // 既に同名エフェクトがチェーンに存在する場合は多重追加しない。
    if (std::find(effectChain_.begin(), effectChain_.end(), name) != effectChain_.end()) {
      return;
    }

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
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dx12_->GetMainDSVHandle();

    // エフェクト適用対象 RT に描画
    dx12_->GetCommandList()->OMSetRenderTargets(
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

    // エフェクト適用対象 RT をクリア
    dx12_->GetCommandList()->ClearRenderTargetView(effectTargetRT_.rtvHandle, clearColor, 0, nullptr);
  }

  void PostEffectManager::BeginDrawNonEffectTarget()
  {
    // nonEffectTargetRT を RENDER_TARGET 状態にして記録
    TransitionResourceWithTracking(
      nonEffectTargetRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dx12_->GetMainDSVHandle();

    // 非適用対象 RT に描画
    dx12_->GetCommandList()->OMSetRenderTargets(
      1,
      &nonEffectTargetRT_.rtvHandle,
      false,
      &dsvHandle);
  }

  void PostEffectManager::Draw()
  {
    ApplyEffectChain();
  }

  void PostEffectManager::DrawFinalResult(bool drawToSwapChain)
  {
    // スワップチェインへの描画（パラメータが true の時のみ）
    if (drawToSwapChain) {

      // 非適用対象 RT をシェーダーリソースに遷移
      TransitionResourceWithTracking(
        nonEffectTargetRT_.resource.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
      );

      // スワップチェインに描画
      dx12_->SetSwapChain();

      // NoEffect を使って単純コピー
      if (effectRegistry_.find("NoEffect") != effectRegistry_.end()) {
        auto& baseEffect = effectRegistry_["NoEffect"];

        // NoEffect にダウンキャスト
        NoEffect* noEffect = dynamic_cast<NoEffect*>(baseEffect.get());
        if (noEffect != nullptr) {
          noEffect->ApplyToBackBuffer(nonEffectTargetRT_.srvIndex);
        }
      }

      TransitionResourceWithTracking(
        nonEffectTargetRT_.resource.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET
      );
    }
    else {
      dx12_->SetSwapChain();

      // 非適用対象 RT をシェーダーリソースに遷移
      TransitionResourceWithTracking(
        nonEffectTargetRT_.resource.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
      );
    }
  }

  void PostEffectManager::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::Begin("PostEffect Manager");

    if (ImGui::BeginTabBar("PostEffectTab")) {
      if (ImGui::BeginTabItem("PostEffect")) {
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
          }
          else {
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
        }
        else {
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
            selectedActiveEffect_ = "";
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
          selectedActiveEffect_ = "";
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
        }
        else {
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

      if (ImGui::BeginTabItem("PostEffectParam")) {
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
    // 古いリソースの状態追跡エントリを削除
    if (effectTargetRT_.resource) {
      resourceStates_.erase(effectTargetRT_.resource.Get());
    }
    if (nonEffectTargetRT_.resource) {
      resourceStates_.erase(nonEffectTargetRT_.resource.Get());
    }
    for (auto& rt : intermediateRTs_) {
      if (rt.resource) {
        resourceStates_.erase(rt.resource.Get());
      }
    }

    // 既存のリソースを解放
    effectTargetRT_.Release();
    nonEffectTargetRT_.Release();

    for (auto& rt : intermediateRTs_) {
      rt.Release();
    }

    // 新しいサイズで再作成
    CreateRenderTextures();

    // 深度バッファの SRV も更新
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      depthSrvIndex_,
      dx12_->GetDepthStencilResource(),
      DXGI_FORMAT_R32_FLOAT,
      1
    );
  }

  bool PostEffectManager::SetEffectParam(const std::string& effectName, const EffectParam& param)
  {
    // エフェクトが存在するかチェック
    auto it = effectRegistry_.find(effectName);
    if (it == effectRegistry_.end()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Effect not found: " + effectName, DebugUIManager::LogType::Error);
#endif
      return false;
    }

    // エフェクトにパラメーターを設定
    bool success = it->second->SetGenericParam(param);

#ifdef _DEBUG
    if (!success) {
      DebugUIManager::GetInstance()->AddLog("Parameter type mismatch for effect: " + effectName, DebugUIManager::LogType::Error);
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

  uint32_t PostEffectManager::GetFinalResultSrvIndex() const
  {
    // 常に nonEffectTargetRT の SRV インデックスを返す
    // DrawFinalResult()で nonEffectTargetRT がスワップチェーンに描画されているため
    return nonEffectTargetRT_.srvIndex;
  }

  ID3D12Resource* PostEffectManager::GetFinalResultResource() const
  {
    return nonEffectTargetRT_.resource.Get();
  }

  //------------------------------- プライベート関数 -------------------------------//

  void PostEffectManager::CreateRenderTextures() {
    // エフェクト適用対象用 RT
    effectTargetRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, kEffectTargetClearColor_);
    effectTargetRT_.resource->SetName(L"EffectTargetRT");
    // 初期状態を設定（レンダーテクスチャは RENDER_TARGET として作成される）
    SetInitialResourceState(effectTargetRT_.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 非適用対象用 RT (最終表示用のため SRV は α=1 固定で読ませ、ImGui 表示での背景透けを防ぐ)
    nonEffectTargetRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, nonEffectTargetClearColor_, true);
    nonEffectTargetRT_.resource->SetName(L"NonEffectTargetRT");
    // 初期状態を設定
    SetInitialResourceState(nonEffectTargetRT_.resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 中間バッファ
    intermediateRTs_.resize(2);
    for (size_t i = 0; i < intermediateRTs_.size(); ++i) {
      intermediateRTs_[i].Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, kEffectTargetClearColor_);
      intermediateRTs_[i].resource->SetName(L"IntermediateRT");
      // 初期状態を設定
      SetInitialResourceState(intermediateRTs_[i].resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
  }

  void PostEffectManager::RegisterEffect(const std::string& name, std::unique_ptr<IPostEffect> effect)
  {
    if (effectRegistry_.find(name) != effectRegistry_.end()) {
      return;
    }
    effectRegistry_[name] = std::move(effect);
    effectRegistry_[name]->Initialize(dx12_, name);

    // 利用可能なエフェクトリストに追加（NoEffect 以外）
    if (name != "NoEffect") {
      availableEffects_.push_back(name);
    }

  }

  void PostEffectManager::ApplyEffectChain()
  {
    // エフェクトチェーンに NoEffect を自動追加せず、空の場合のみ NoEffect を使用
    std::vector<std::string> actualChain;

    if (effectChain_.empty()) {
      actualChain.push_back("NoEffect");
    }
    else {
      actualChain = effectChain_;
    }

    // 最初の入力はエフェクト適用対象 RT
    TransitionResourceWithTracking(
      effectTargetRT_.resource.Get(),
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
      dx12_->TransitionResourceState(
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        dx12_->GetDepthStencilResource()
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
        // 最後のエフェクトは nonEffectTargetRT に描画するため、RENDER_TARGET 状態に遷移
        TransitionResourceWithTracking(
          nonEffectTargetRT_.resource.Get(),
          D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        dstRtvHandle = nonEffectTargetRT_.rtvHandle;
      }
      else {
        int bufferIndex = i % 2;
        auto& intermediateRT = intermediateRTs_[bufferIndex];

        if (intermediateRTStates[bufferIndex]) {
          TransitionResourceWithTracking(
            intermediateRT.resource.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET
          );
          intermediateRTStates[bufferIndex] = false;
        }

        dstRtvHandle = intermediateRT.rtvHandle;
      }

      if (effectName == "DepthBasedOutline") {
        auto it = effectRegistry_.find(effectName);
        if (it != effectRegistry_.end()) {
          auto depthEffect = dynamic_cast<DepthBasedOutline*>(it->second.get());
          if (depthEffect) {
            depthEffect->SetInvProjectionMatrix(Mat4x4::Inverse(camera_->GetProjectionMatrix()));
          }
        }
      }

      if (effectName == "Dissolve") {
        effect->Apply(srcSrvIndex, dstRtvHandle, dissolveMaskSrvIndex_, nonEffectTargetClearColor_);
      }
      else {
        effect->Apply(srcSrvIndex, dstRtvHandle, depthSrvIndex_, nonEffectTargetClearColor_);
      }


      if (!isLastEffect) {
        int bufferIndex = i % 2;
        auto& usedRT = intermediateRTs_[bufferIndex];

        TransitionResourceWithTracking(
          usedRT.resource.Get(),
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        intermediateRTStates[bufferIndex] = true;

        srcSrvIndex = usedRT.srvIndex;
      }
    }

    // リソース復元
    if (needsDepthBuffer) {
      dx12_->TransitionResourceState(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        dx12_->GetDepthStencilResource()
      );
    }

    TransitionResourceWithTracking(
      effectTargetRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    for (size_t i = 0; i < intermediateRTs_.size(); ++i) {
      if (intermediateRTStates[i]) {
        auto& rt = intermediateRTs_[i];
        TransitionResourceWithTracking(
          rt.resource.Get(),
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
    }
    else {
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
    stateBefore;
    // この関数は廃止予定。代わりに TransitionResourceWithTracking を使用
    TransitionResourceWithTracking(effectTargetRT_.resource.Get(), stateAfter);
  }

  void PostEffectManager::SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
  {
    // stateBefore は使用せず、TransitionResourceWithTracking で状態追跡を使用
    stateBefore; // 未使用パラメータの警告を抑制
    TransitionResourceWithTracking(resource, stateAfter);
  }

  void PostEffectManager::TransitionResourceWithTracking(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState)
  {
    // 現在の状態を取得（未追跡の場合は COMMON と仮定）
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    auto it = resourceStates_.find(resource);
    if (it != resourceStates_.end()) {
      currentState = it->second;
    }

    // 状態が同じなら何もしない
    if (currentState == newState) {
      return;
    }

    // バリア遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = newState;
    dx12_->GetCommandList()->ResourceBarrier(1, &barrier);

    // 新しい状態を記録
    resourceStates_[resource] = newState;
  }

  D3D12_RESOURCE_STATES PostEffectManager::GetResourceState(ID3D12Resource* resource) const
  {
    auto it = resourceStates_.find(resource);
    if (it != resourceStates_.end()) {
      return it->second;
    }
    // 未追跡の場合は COMMON を返す
    return D3D12_RESOURCE_STATE_COMMON;
  }

  void PostEffectManager::SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState)
  {
    // バリア遷移なしで、状態のみを記録
    resourceStates_[resource] = initialState;
  }

  void PostEffectManager::Update(float deltaTime)
  {
    std::vector<std::string> toRemove;

    for (auto& [effectName, info] : temporaryEffects_) {
      info.elapsedTime += deltaTime;

      if (info.elapsedTime >= info.duration) {
        toRemove.push_back(effectName);
      }
      else {
        float progress = info.elapsedTime / info.duration;
        float easedProgress = ApplyEasing(progress, info.easing);
        float fadeFactor = 1.0f - easedProgress;

        EffectParam fadedParam = ApplyFadeToParam(info.baseParam, fadeFactor);
        SetEffectParam(effectName, fadedParam);
      }
    }

    for (const auto& name : toRemove) {
      temporaryEffects_.erase(name);
      RemoveEffectFromChain(name);
    }
  }

  void PostEffectManager::CancelTemporaryEffect(const std::string& effectName)
  {
    auto it = temporaryEffects_.find(effectName);
    if (it != temporaryEffects_.end()) {
      temporaryEffects_.erase(it);
      RemoveEffectFromChain(effectName);
    }
  }

  bool PostEffectManager::IsTemporaryEffectActive(const std::string& effectName) const
  {
    return temporaryEffects_.find(effectName) != temporaryEffects_.end();
  }

  float PostEffectManager::ApplyEasing(float t, EasingType type) const
  {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
    case EasingType::Linear:
      return Ease::Linear(t);
    case EasingType::EaseOut:
      return Ease::OutQuad(t);
    case EasingType::EaseIn:
      return Ease::InQuad(t);
    case EasingType::EaseInOut:
      return Ease::InOutQuad(t);
    default:
      return t;
    }
  }

  EffectParam PostEffectManager::ApplyFadeToParam(const EffectParam& param, float fadeFactor) const
  {
    return std::visit([fadeFactor](auto&& arg) -> EffectParam {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, VignetteParam>) {
        VignetteParam result = arg;
        result.power *= fadeFactor;
        return result;
      }
      else if constexpr (std::is_same_v<T, BloomParam>) {
        BloomParam result = arg;
        result.intensity *= fadeFactor;
        return result;
      }
      else if constexpr (std::is_same_v<T, RadialBlurParam>) {
        RadialBlurParam result = arg;
        result.blurWidth *= fadeFactor;
        return result;
      }
      else if constexpr (std::is_same_v<T, RGBSplitParam>) {
        RGBSplitParam result = arg;
        result.intensity *= fadeFactor;
        return result;
      }
      else if constexpr (std::is_same_v<T, GaussianBlurParam>) {
        GaussianBlurParam result = arg;
        result.sigma *= fadeFactor;
        return result;
      }
      else {
        return arg;
      }
      }, param);
  }

} // namespace Tako