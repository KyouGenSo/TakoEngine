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
#include "FXAA.h"
#include "EaseFunc.h"
#include "Logger.h"

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <string_view>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  using json = nlohmann::json;

  // プロファイル保存用。ADL で見つかるよう型と同じ名前空間に置き、static で他翻訳単位と衝突させない
  static void to_json(json& j, const Vector2& v) { j = { v.x, v.y }; }
  static void from_json(const json& j, Vector2& v) { v = { j.at(0).get<float>(), j.at(1).get<float>() }; }
  static void to_json(json& j, const Vector3& v) { j = { v.x, v.y, v.z }; }
  static void from_json(const json& j, Vector3& v) { v = { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() }; }
  static void to_json(json& j, const Vector4& v) { j = { v.x, v.y, v.z, v.w }; }
  static void from_json(const json& j, Vector4& v) { v = { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }

  // 実行時に決まる値（投影逆行列・画面サイズ・時間・ブラー方向）と padding は保存しない
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VignetteParam, power, range, color)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RadialBlurParam, center, blurWidth, sampleCount)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RGBSplitParam, redOffset, greenOffset, blueOffset, intensity)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BWFilterParam, threshold)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LuminanceOutlineParam, outlineThickness)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DepthOutlineParam, outlineThickness)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FogParam, color, density)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DissolveParam, threshold, edgeThickness, edgeColor)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HalfToneParam, dotSize, contrast, angle, dotPattern, colorMode, threshold)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GaussianBlurParam, sigma, kernelSize)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HighLumExtrcatParam, threshold)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BloomCombineParam, intensity)

  namespace {

    // PostEffectType の並びと一致させること。シェーダーファイル名と ImGui 表示名を兼ねる
    constexpr const char* kEffectNames[] = {
      "NoEffect", "GrayScale", "Vignette", "RadialBlur", "RGBSplit", "BWFilter", "LuminanceBasedOutline",
      "DepthBasedOutline", "Fog", "Bloom", "Dissolve", "WhiteNoise", "HalfTone", "GaussianBlur", "FXAA",
    };
    static_assert(std::size(kEffectNames) == static_cast<size_t>(PostEffectType::Count));

    const std::filesystem::path kProfileDirectory = "resources/Json/PostEffects";

    const char* EffectName(PostEffectType type) {
      return kEffectNames[static_cast<size_t>(type)];
    }

    std::optional<PostEffectType> FindEffectType(std::string_view name) {
      const auto it = std::ranges::find_if(kEffectNames, [name](const char* effectName) { return name == effectName; });
      if (it == std::end(kEffectNames)) {
        return std::nullopt;
      }
      return static_cast<PostEffectType>(it - std::begin(kEffectNames));
    }

    std::filesystem::path ProfilePath(const std::string& name) {
      return kProfileDirectory / (name + ".json");
    }

    // to_json が定義された（保存対象の）型だけ扱う
    template<class T>
    concept SerializableParam = requires(json& j, const T& param) { to_json(j, param); };

    void SetViewportSize(ID3D12GraphicsCommandList* commandList, uint32_t width, uint32_t height) {
      const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
      const D3D12_RECT     scissor{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
      commandList->RSSetViewports(1, &viewport);
      commandList->RSSetScissorRects(1, &scissor);
    }

#ifdef _DEBUG
    std::vector<std::string> ProfileNames() {
      std::vector<std::string> names;
      std::error_code ec;
      for (const auto& entry : std::filesystem::directory_iterator(kProfileDirectory, ec)) {
        if (entry.path().extension() == ".json") {
          names.push_back(entry.path().stem().string());
        }
      }
      return names;
    }
#endif

  } // anonymous namespace

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
    RegisterEffect(PostEffectType::NoEffect, std::make_unique<NoEffect>());
    RegisterEffect(PostEffectType::GrayScale, std::make_unique<GrayScale>());
    RegisterEffect(PostEffectType::Vignette, std::make_unique<Vignette>());
    RegisterEffect(PostEffectType::RadialBlur, std::make_unique<RadialBlur>());
    RegisterEffect(PostEffectType::RGBSplit, std::make_unique<RGBSplit>());
    RegisterEffect(PostEffectType::BWFilter, std::make_unique<BWFilter>());
    RegisterEffect(PostEffectType::LuminanceBasedOutline, std::make_unique<LuminanceBasedOutline>());
    RegisterEffect(PostEffectType::DepthBasedOutline, std::make_unique<DepthBasedOutline>());
    RegisterEffect(PostEffectType::Fog, std::make_unique<Fog>());
    RegisterEffect(PostEffectType::Bloom, std::make_unique<Bloom>());
    RegisterEffect(PostEffectType::Dissolve, std::make_unique<Dissolve>());
    RegisterEffect(PostEffectType::WhiteNoise, std::make_unique<WhiteNoise>());
    RegisterEffect(PostEffectType::HalfTone, std::make_unique<HalfTone>());
    RegisterEffect(PostEffectType::GaussianBlur, std::make_unique<GaussianBlur>());
    RegisterEffect(PostEffectType::FXAA, std::make_unique<FXAA>());

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

  void PostEffectManager::AddEffectToChain(PostEffectType type)
  {
    // 既に同じエフェクトがチェーンに存在する場合は多重追加しない。
    if (IsEffectInChain(type)) {
      return;
    }

    if (effectRegistry_[static_cast<size_t>(type)]) {
      effectChain_.push_back(type);
    }
  }

  void PostEffectManager::RemoveEffectFromChain(PostEffectType type)
  {
    effectChain_.erase(
      std::remove(effectChain_.begin(), effectChain_.end(), type),
      effectChain_.end()
    );
  }

  void PostEffectManager::ClearEffectChain()
  {
    effectChain_.clear();
  }

  void PostEffectManager::BeginDrawEffectTarget()
  {
    // エフェクト適用対象 RT に描画
    dx12_->BindSceneRenderTarget({ effectTargetRT_.rtvHandle, dx12_->GetMainDSVHandle(), dx12_->GetSceneWidth(), dx12_->GetSceneHeight() });

    // エフェクト適用対象 RT をクリア
    dx12_->GetCommandList()->ClearRenderTargetView(effectTargetRT_.rtvHandle, &clearColor_.x, 0, nullptr);
  }

  void PostEffectManager::BeginDrawNonEffectTarget()
  {
    // nonEffectTargetRT を RENDER_TARGET 状態にして記録
    TransitionResourceWithTracking(
      nonEffectTargetRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    // 非適用対象 RT に描画
    dx12_->BindSceneRenderTarget({ nonEffectTargetRT_.rtvHandle, dx12_->GetNonEffectDSVHandle(), static_cast<uint32_t>(WinApp::clientWidth), static_cast<uint32_t>(WinApp::clientHeight) });
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
      NoEffect* noEffect = dynamic_cast<NoEffect*>(effectRegistry_[static_cast<size_t>(PostEffectType::NoEffect)].get());
      if (noEffect != nullptr) {
        noEffect->ApplyToBackBuffer(nonEffectTargetRT_.srvIndex);
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
        ImGui::SeparatorText("Profile");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##ProfileName", profileName_.data(), profileName_.size());
        ImGui::SameLine();
        const bool hasName = profileName_[0] != '\0';
        ImGui::BeginDisabled(!hasName);
        if (ImGui::Button("Save")) {
          const bool saved = SaveProfile(profileName_.data());
          DebugUIManager::GetInstance()->AddLog(std::format("PostEffect profile {}: {}", saved ? "saved" : "save failed", profileName_.data()),
                                                saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("Load", "Select...")) {
          for (const std::string& name : ProfileNames()) {
            if (ImGui::Selectable(name.c_str())) {
              const bool loaded = LoadProfile(name);
              DebugUIManager::GetInstance()->AddLog(std::format("PostEffect profile {}: {}", loaded ? "loaded" : "load failed", name),
                                                    loaded ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
              std::ranges::fill(profileName_, '\0');
              name.copy(profileName_.data(), profileName_.size() - 1);
            }
          }
          ImGui::EndCombo();
        }

        ImGui::Text("Post Effects Configuration");
        ImGui::Separator();

        // 2カラムレイアウト
        ImGui::Columns(2, "EffectColumns", true);

        // === 左パネル: 利用可能なエフェクト ===
        ImGui::Text("Available Effects");
        ImGui::Separator();

        // 利用可能エフェクトリスト
        ImGui::BeginChild("AvailableList", ImVec2(0, 200), true);
        // NoEffect はチェーンが空のときの素通し用、FXAA はアンチエイリアス設定で掛けるので一覧に出さない
        for (size_t i = static_cast<size_t>(PostEffectType::NoEffect) + 1; i < static_cast<size_t>(PostEffectType::Count); ++i) {
          const PostEffectType type = static_cast<PostEffectType>(i);
          if (type == PostEffectType::FXAA) {
            continue;
          }
          bool isSelected = (selectedAvailableEffect_ == type);

          if (ImGui::Selectable(EffectName(type), isSelected)) {
            selectedAvailableEffect_ = type;
          }
        }
        ImGui::EndChild();

        // 適用ボタン
        bool canApply = selectedAvailableEffect_.has_value() &&
          !IsEffectInChain(*selectedAvailableEffect_);

        if (!canApply) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Apply Effect", ImVec2(-1, 0))) {
          if (selectedAvailableEffect_) {
            AddEffectToChain(*selectedAvailableEffect_);
          }
        }

        if (!canApply) {
          ImGui::EndDisabled();
        }

        // 状態表示
        if (selectedAvailableEffect_) {
          if (IsEffectInChain(*selectedAvailableEffect_)) {
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
            const PostEffectType type = effectChain_[i];
            bool isSelected = (selectedActiveEffect_ == type);

            // エフェクト名に順序番号を付けて表示
            std::string displayName = std::to_string(i + 1) + ". " + EffectName(type);

            if (ImGui::Selectable(displayName.c_str(), isSelected)) {
              selectedActiveEffect_ = type;
            }
          }
        }

        ImGui::EndChild();

        // 操作ボタン
        bool hasSelection = selectedActiveEffect_.has_value() &&
          IsEffectInChain(*selectedActiveEffect_);

        // Remove ボタン
        if (!hasSelection) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Remove Effect", ImVec2(-1, 0))) {
          if (selectedActiveEffect_) {
            RemoveEffectFromChain(*selectedActiveEffect_);
            selectedActiveEffect_.reset();
          }
        }

        if (!hasSelection) {
          ImGui::EndDisabled();
        }

        ImGui::Spacing();

        // 順序変更ボタン
        ImGui::BeginGroup();

        bool canMoveUp = hasSelection && GetEffectPosition(*selectedActiveEffect_) > 0;
        bool canMoveDown = hasSelection &&
          GetEffectPosition(*selectedActiveEffect_) < static_cast<int>(GetEffectChainSize()) - 1;

        if (!canMoveUp) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Move Up", ImVec2(80, 0))) {
          MoveEffectUp(*selectedActiveEffect_);
        }

        if (!canMoveUp) {
          ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (!canMoveDown) {
          ImGui::BeginDisabled();
        }

        if (ImGui::Button("Move Down", ImVec2(-1, 0))) {
          MoveEffectDown(*selectedActiveEffect_);
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
          selectedActiveEffect_.reset();
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
        if (selectedActiveEffect_) {
          int position = GetEffectPosition(*selectedActiveEffect_);
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
            ImGui::Text("%s", EffectName(effectChain_[i]));
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

  bool PostEffectManager::SetEffectParam(PostEffectType type, const EffectParam& param)
  {
    // エフェクトが存在するかチェック
    const auto& effect = effectRegistry_[static_cast<size_t>(type)];
    if (!effect) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(std::string("Effect not registered: ") + EffectName(type), DebugUIManager::LogType::Error);
#endif
      return false;
    }

    // エフェクトにパラメーターを設定
    bool success = effect->SetGenericParam(param);

#ifdef _DEBUG
    if (!success) {
      DebugUIManager::GetInstance()->AddLog(std::string("Parameter type mismatch for effect: ") + EffectName(type), DebugUIManager::LogType::Error);
    }
#endif

    return success;
  }

  void PostEffectManager::SetDissolveBaseTex(const std::string& textureName)
  {
    auto dissolveEffect = dynamic_cast<Dissolve*>(effectRegistry_[static_cast<size_t>(PostEffectType::Dissolve)].get());
    if (dissolveEffect) {
      dissolveEffect->SetBaseTextureSrvIndex(TextureManager::GetInstance()->GetSRVIndex(textureName));
    }
  }

  void PostEffectManager::SetClearColor(const Vector4& color)
  {
    clearColor_ = color;
    // 作成時と異なる色でクリアするとデバッグレイヤーの警告で止まるため、次フレーム先頭で RT ごと作り直す
    dx12_->RequestRenderTargetRebuild();
  }

  bool PostEffectManager::SaveProfile(const std::string& name) const
  {
    json chain  = json::array();
    json params = json::object();
    for (PostEffectType type : effectChain_) {
      if (temporaryEffects_.contains(type)) {
        continue;
      }
      chain.push_back(EffectName(type));

      json values = json::array();
      for (const EffectParam& param : effectRegistry_[static_cast<size_t>(type)]->GetGenericParams()) {
        std::visit([&values]<class T>(const T& value) {
          if constexpr (SerializableParam<T>) {
            values.push_back(value);
          }
        }, param);
      }
      if (!values.empty()) {
        params[EffectName(type)] = std::move(values);
      }
    }

    std::error_code ec;
    std::filesystem::create_directories(kProfileDirectory, ec);
    std::ofstream ofs(ProfilePath(name));
    if (!ofs) {
      Logger::Log("PostEffect profile save failed: %s", name.c_str());
      return false;
    }
    ofs << std::setw(4) << json{ { "Chain", chain }, { "Params", params } } << std::endl;
    return true;
  }

  bool PostEffectManager::LoadProfile(const std::string& name)
  {
    std::ifstream ifs(ProfilePath(name));
    if (!ifs) {
      Logger::Log("PostEffect profile not found: %s", name.c_str());
      return false;
    }

    // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
    try {
      const json root = json::parse(ifs);

      ClearEffectChain();
      for (const json& effectName : root.at("Chain")) {
        if (const auto type = FindEffectType(effectName.get<std::string>())) {
          AddEffectToChain(*type);
        }
      }

      for (const auto& item : root.at("Params").items()) {
        const auto type = FindEffectType(item.key());
        if (!type) {
          continue;
        }
        const json& values = item.value();
        IPostEffect* effect = effectRegistry_[static_cast<size_t>(*type)].get();
        // 現在値に上書きするので、保存していない実行時の値はそのまま残る
        std::vector<EffectParam> current = effect->GetGenericParams();
        for (size_t i = 0; i < (std::min)(current.size(), values.size()); ++i) {
          std::visit([&values, i]<class T>(T& value) {
            if constexpr (SerializableParam<T>) {
              values[i].get_to(value);
            }
          }, current[i]);
          effect->SetGenericParam(current[i]);
        }
      }
      return true;
    }
    catch (const json::exception& e) {
      Logger::Log("PostEffect profile load failed: %s (%s)", name.c_str(), e.what());
      return false;
    }
  }

  bool PostEffectManager::MoveEffectUp(PostEffectType type)
  {
    auto it = std::find(effectChain_.begin(), effectChain_.end(), type);
    if (it == effectChain_.end() || it == effectChain_.begin()) {
      return false; // エフェクトが見つからない、または既に最上位
    }

    // 一つ上の要素と交換
    std::iter_swap(it, it - 1);
    return true;
  }

  bool PostEffectManager::MoveEffectDown(PostEffectType type)
  {
    auto it = std::find(effectChain_.begin(), effectChain_.end(), type);
    if (it == effectChain_.end() || it == effectChain_.end() - 1) {
      return false; // エフェクトが見つからない、または既に最下位
    }

    // 一つ下の要素と交換
    std::iter_swap(it, it + 1);
    return true;
  }

  bool PostEffectManager::MoveEffectToPosition(PostEffectType type, int newPosition)
  {
    // 範囲チェック
    if (newPosition < 0 || newPosition >= static_cast<int>(effectChain_.size())) {
      return false;
    }

    // 現在の位置を取得
    auto it = std::find(effectChain_.begin(), effectChain_.end(), type);
    if (it == effectChain_.end()) {
      return false; // エフェクトが見つからない
    }

    int currentPosition = static_cast<int>(std::distance(effectChain_.begin(), it));
    if (currentPosition == newPosition) {
      return true; // 既に目的の位置にある
    }

    // エフェクトを一時的に保存して削除
    const PostEffectType tempEffect = *it;
    effectChain_.erase(it);

    // 新しい位置に挿入
    if (newPosition > currentPosition) {
      // 削除により位置がずれるため調整
      newPosition--;
    }
    effectChain_.insert(effectChain_.begin() + newPosition, tempEffect);

    return true;
  }

  bool PostEffectManager::SwapEffects(PostEffectType type1, PostEffectType type2)
  {
    auto it1 = std::find(effectChain_.begin(), effectChain_.end(), type1);
    auto it2 = std::find(effectChain_.begin(), effectChain_.end(), type2);

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

  int PostEffectManager::GetEffectPosition(PostEffectType type) const
  {
    auto it = std::find(effectChain_.begin(), effectChain_.end(), type);
    if (it == effectChain_.end()) {
      return -1; // エフェクトが見つからない
    }

    return static_cast<int>(std::distance(effectChain_.begin(), it));
  }

  bool PostEffectManager::IsEffectInChain(PostEffectType type) const
  {
    return std::find(effectChain_.begin(), effectChain_.end(), type) != effectChain_.end();
  }

  size_t PostEffectManager::GetEffectChainSize() const
  {
    return effectChain_.size();
  }

  std::optional<PostEffectType> PostEffectManager::GetEffectAtPosition(int position) const
  {
    if (position < 0 || position >= static_cast<int>(effectChain_.size())) {
      return std::nullopt; // 無効な位置
    }

    return effectChain_[position];
  }

  std::vector<PostEffectType> PostEffectManager::GetEffectChain() const
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
    // エフェクト適用対象用 RT と中間バッファはシーン解像度、最終表示用の非適用対象 RT はウィンドウ解像度
    const uint32_t sceneWidth  = dx12_->GetSceneWidth();
    const uint32_t sceneHeight = dx12_->GetSceneHeight();

    // エフェクト適用対象用 RT
    effectTargetRT_.Create(dx12_, sceneWidth, sceneHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor_);
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
      intermediateRTs_[i].Create(dx12_, sceneWidth, sceneHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor_);
      intermediateRTs_[i].resource->SetName(L"IntermediateRT");
      // 初期状態を設定
      SetInitialResourceState(intermediateRTs_[i].resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
  }

  void PostEffectManager::RegisterEffect(PostEffectType type, std::unique_ptr<IPostEffect> effect)
  {
    auto& slot = effectRegistry_[static_cast<size_t>(type)];
    if (slot) {
      return;
    }
    slot = std::move(effect);
    slot->Initialize(dx12_, EffectName(type));
  }

  void PostEffectManager::ApplyEffectChain()
  {
    const uint32_t sceneWidth  = dx12_->GetSceneWidth();
    const uint32_t sceneHeight = dx12_->GetSceneHeight();
    const bool     isScaled    = sceneWidth != static_cast<uint32_t>(WinApp::clientWidth) || sceneHeight != static_cast<uint32_t>(WinApp::clientHeight);

    std::vector<PostEffectType> actualChain = effectChain_;
    if (antiAliasing_ == AntiAliasing::FXAA) {
      actualChain.push_back(PostEffectType::FXAA);
    }
    // 最終パスはウィンドウ解像度の nonEffectTarget へ書くため、縮小解像度ならエフェクトとは別に拡大コピーを最後に足す
    if (actualChain.empty() || isScaled) {
      actualChain.push_back(PostEffectType::NoEffect);
    }

    // シーン描画中の途中パスがビューポートを変えていても、チェーンはシーン解像度で回す
    SetViewportSize(dx12_->GetCommandList(), sceneWidth, sceneHeight);

    // 最初の入力はエフェクト適用対象 RT
    TransitionResourceWithTracking(
      effectTargetRT_.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    // 深度バッファチェック
    bool needsDepthBuffer = false;
    for (PostEffectType type : actualChain) {
      if (effectRegistry_[static_cast<size_t>(type)]->RequiresDepthBuffer()) {
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
      const PostEffectType type = actualChain[i];
      auto& effect = effectRegistry_[static_cast<size_t>(type)];

      if (!effect) continue;

      bool isLastEffect = (i == actualChain.size() - 1);

      if (isLastEffect) {
        // 最後のエフェクトは nonEffectTargetRT に描画するため、RENDER_TARGET 状態に遷移
        TransitionResourceWithTracking(
          nonEffectTargetRT_.resource.Get(),
          D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        dstRtvHandle = nonEffectTargetRT_.rtvHandle;
        SetViewportSize(dx12_->GetCommandList(), static_cast<uint32_t>(WinApp::clientWidth), static_cast<uint32_t>(WinApp::clientHeight));
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

      if (type == PostEffectType::DepthBasedOutline) {
        auto depthEffect = dynamic_cast<DepthBasedOutline*>(effect.get());
        if (depthEffect) {
          depthEffect->SetInvProjectionMatrix(Mat4x4::Inverse(camera_->GetProjectionMatrix()));
        }
      }

      if (type == PostEffectType::Dissolve) {
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
        const PostEffectType type = effectChain_[i];
        std::string headerName = "[" + std::to_string(i + 1) + "] " + EffectName(type);

        if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
          if (const auto& effect = effectRegistry_[static_cast<size_t>(type)]) {
            effect->DrawImgui();
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
    std::vector<PostEffectType> toRemove;

    for (auto& [type, info] : temporaryEffects_) {
      info.elapsedTime += deltaTime;

      if (info.elapsedTime >= info.duration) {
        toRemove.push_back(type);
      }
      else {
        float progress = info.elapsedTime / info.duration;
        float easedProgress = ApplyEasing(progress, info.easing);
        float fadeFactor = 1.0f - easedProgress;

        EffectParam fadedParam = ApplyFadeToParam(info.baseParam, fadeFactor);
        SetEffectParam(type, fadedParam);
      }
    }

    for (PostEffectType type : toRemove) {
      temporaryEffects_.erase(type);
      RemoveEffectFromChain(type);
    }
  }

  void PostEffectManager::CancelTemporaryEffect(PostEffectType type)
  {
    auto it = temporaryEffects_.find(type);
    if (it != temporaryEffects_.end()) {
      temporaryEffects_.erase(it);
      RemoveEffectFromChain(type);
    }
  }

  bool PostEffectManager::IsTemporaryEffectActive(PostEffectType type) const
  {
    return temporaryEffects_.find(type) != temporaryEffects_.end();
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