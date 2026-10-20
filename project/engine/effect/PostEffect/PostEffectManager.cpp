#include "PostEffectManager.h"
#include "Camera.h"
#include "DX12Basic.h"
#include "EaseFunc.h"
#include "FrameTimer.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "TextureManager.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>

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
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BloomParam, intensity, threshold, sigma, kernelSize)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RadialBlurParam, center, blurWidth, sampleCount)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RGBSplitParam, redOffset, greenOffset, blueOffset, intensity)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BWFilterParam, threshold)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LuminanceOutlineParam, outlineThickness)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DepthOutlineParam, outlineThickness)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FogParam, color, density)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DissolveParam, threshold, edgeThickness, edgeColor)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HalfToneParam, dotSize, contrast, angle, dotPattern, colorMode, threshold)
  NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GaussianBlurParam, sigma, kernelSize)

  namespace {

    const std::filesystem::path kProfileDirectory = "resources/Json/PostEffects";

    std::filesystem::path ProfilePath(const std::string& name) {
      return kProfileDirectory / (name + ".json");
    }

    // to_json が定義された（保存対象の）型だけ扱う
    template<class T>
    concept SerializableParam = requires(json& j, const T& param) { to_json(j, param); };

    /// <summary>
    /// input を direction の向きにぼかして output へ書く
    /// </summary>
    void BlurPass(const PostEffectFrame& frame, RenderTexture& input, RenderTexture& output, float sigma, int kernelSize, const Vector2& direction)
    {
      frame.graph->AddPass({ RenderGraph::Read(input.resource), RenderGraph::Write(output.resource) }, [&] {
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, L"GaussianBlur.PS.hlsl", output);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        binder.BindConstants("gParam"_id, GaussianBlurParam{ sigma, kernelSize, direction });
        DrawFullScreen(frame);
      });
    }

    void ExecuteGaussianBlur(const PostEffectFrame& frame, const EffectParam& param, RenderTexture& input, RenderTexture& output)
    {
      const GaussianBlurParam& blur = std::get<GaussianBlurParam>(param);
      RenderTexture& vertical = frame.graph->Acquire(RenderGraph::DescOf(input));
      BlurPass(frame, input, vertical, blur.sigma, blur.kernelSize, { 0.0f, 1.0f });
      BlurPass(frame, vertical, output, blur.sigma, blur.kernelSize, { 1.0f, 0.0f });
      frame.graph->Release(vertical);
    }

    /// <summary>
    /// 高輝度部分を抜き出してぼかし、元の画像に加算する
    /// </summary>
    void ExecuteBloom(const PostEffectFrame& frame, const EffectParam& param, RenderTexture& input, RenderTexture& output)
    {
      const BloomParam& bloom = std::get<BloomParam>(param);
      RenderGraph&      graph = *frame.graph;
      RenderTexture&    bright = graph.Acquire(RenderGraph::DescOf(input));
      RenderTexture&    blurX  = graph.Acquire(RenderGraph::DescOf(input));
      RenderTexture&    blurY  = graph.Acquire(RenderGraph::DescOf(input));

      graph.AddPass({ RenderGraph::Read(input.resource), RenderGraph::Write(bright.resource) }, [&] {
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, L"ThresholdExtract.PS.hlsl", bright);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        binder.BindConstants("gParam"_id, bloom.threshold);
        DrawFullScreen(frame);
      });
      BlurPass(frame, bright, blurX, bloom.sigma, bloom.kernelSize, { 1.0f, 0.0f });
      BlurPass(frame, blurX, blurY, bloom.sigma, bloom.kernelSize, { 0.0f, 1.0f });
      graph.AddPass({ RenderGraph::Read(input.resource), RenderGraph::Read(blurY.resource), RenderGraph::Write(output.resource) }, [&] {
        CommandContext::ShaderBinder binder = BeginFullScreen(frame, L"BloomCombine.PS.hlsl", output);
        binder.BindSRV("gTexture"_id, input.srvIndex);
        binder.BindSRV("gBloomTexture"_id, blurY.srvIndex);
        binder.BindConstants("gParam"_id, bloom.intensity);
        DrawFullScreen(frame);
      });

      graph.Release(bright);
      graph.Release(blurX);
      graph.Release(blurY);
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

    // パラメータの型ごとの編集 UI
    void DrawParam(std::monostate&) {}
    void DrawParam(WhiteNoiseParam&) {}

    void DrawParam(VignetteParam& param)
    {
      ImGui::DragFloat("Vignette Power", &param.power, 0.01f, 0.0f, 10.0f);
      ImGui::DragFloat("Vignette Range", &param.range, 0.01f, 0.0f, 100.0f);
      ImGui::ColorEdit3("Vignette Color", &param.color.x);
    }

    void DrawParam(BloomParam& param)
    {
      ImGui::DragFloat("Bloom Intensity", &param.intensity, 0.01f, 0.0f, 10.0f);
      ImGui::DragFloat("Bloom Threshold", &param.threshold, 0.01f, 0.0f, 1.0f);
      ImGui::DragFloat("Bloom Sigma", &param.sigma, 0.01f, 0.0f, 50.0f);
      ImGui::DragInt("Bloom Kernel Size", &param.kernelSize, 1.0f, 1, 100);
    }

    void DrawParam(FogParam& param)
    {
      ImGui::ColorEdit4("FogColor", &param.color.x);
      ImGui::DragFloat("Density", &param.density, 0.001f, 0.0f, 1.0f);
    }

    void DrawParam(RadialBlurParam& param)
    {
      ImGui::DragFloat2("RadialBlur Center", &param.center.x, 0.01f, 0.0f, 1.0f, "%.3f");
      ImGui::DragFloat("RadialBlur Width", &param.blurWidth, 0.001f, 0.0f, 1.0f, "%.3f");
      ImGui::DragInt("RadialBlur Sample Count", &param.sampleCount, 1, 1, 1024, "%d");
    }

    void DrawParam(BWFilterParam& param)
    {
      ImGui::DragFloat("BWFilter Threshold", &param.threshold, 0.01f, 0.0f, 1.0f, "%.2f");
    }

    void DrawParam(RGBSplitParam& param)
    {
      ImGui::DragFloat2("Red Offset", &param.redOffset.x, 0.01f, -1.0f, 1.0f);
      ImGui::DragFloat2("Green Offset", &param.greenOffset.x, 0.01f, -1.0f, 1.0f);
      ImGui::DragFloat2("Blue Offset", &param.blueOffset.x, 0.01f, -1.0f, 1.0f);
      ImGui::DragFloat("RGBSplit Intensity", &param.intensity, 0.01f, 0.0f, 1.0f);
    }

    void DrawParam(LuminanceOutlineParam& param)
    {
      ImGui::DragFloat("LuminanceBasedOutline Thickness", &param.outlineThickness, 0.01f, 0.0f, 100.0f, "%.2f");
    }

    void DrawParam(DepthOutlineParam& param)
    {
      ImGui::DragFloat("DepthBasedOutline Thickness", &param.outlineThickness, 0.01f, 0.0f, 100.0f, "%.2f");
    }

    void DrawParam(DissolveParam& param)
    {
      ImGui::DragFloat("Dissolve Threshold", &param.threshold, 0.01f, 0.0f, 1.0f);
      ImGui::DragFloat("Edge Thickness", &param.edgeThickness, 0.01f, 0.0f, 1.0f);
      ImGui::ColorEdit4("Edge Color", &param.edgeColor.x);
    }

    void DrawParam(HalfToneParam& param)
    {
      ImGui::DragFloat("Dot Size", &param.dotSize, 1.0f, 1.0f, 50.0f);
      ImGui::DragFloat("Contrast", &param.contrast, 0.01f, 0.1f, 3.0f);
      ImGui::DragFloat("Angle", &param.angle, 0.01f, 0.0f, 6.28f);
      ImGui::DragFloat("Threshold", &param.threshold, 0.01f, 0.0f, 1.0f);

      const char* patterns[] = { "Circle", "Square", "Diamond" };
      ImGui::Combo("Dot Pattern", &param.dotPattern, patterns, 3);

      const char* colorModes[] = { "Monochrome", "CMYK" };
      ImGui::Combo("Color Mode", &param.colorMode, colorModes, 2);
    }

    void DrawParam(GaussianBlurParam& param)
    {
      ImGui::DragFloat("Blur Sigma", &param.sigma, 0.01f, 0.0f, 50.0f);
      ImGui::DragInt("Blur Kernel Size", &param.kernelSize, 1.0f, 1, 100);
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
    RegisterBuiltinEffects();
  }

  void PostEffectManager::Finalize()
  {
    instance_.reset();
  }

  uint32_t PostEffectManager::AddEffect(std::string_view name, uint32_t maskId)
  {
    const PostEffectDefinition* definition = FindPostEffect(name);
    if (!definition) {
      Logger::Log("PostEffect not registered: %.*s", static_cast<int>(name.size()), name.data());
      return 0;
    }
    return stack_.Add(name, definition->defaultParam, maskId);
  }

  uint32_t PostEffectManager::AddEffect(std::string_view name, const EffectParam& param, uint32_t maskId)
  {
    const uint32_t id = AddEffect(name, maskId);
    if (id != 0) {
      SetEffectParam(id, param);
    }
    return id;
  }

  void PostEffectManager::RemoveEffect(uint32_t id)
  {
    stack_.Remove(id);
    temporaryEffects_.erase(id);
  }

  void PostEffectManager::ClearEffects()
  {
    stack_.Clear();
    temporaryEffects_.clear();
  }

  uint32_t PostEffectManager::ApplyTemporaryEffect(std::string_view name, float duration, const EffectParam& param, EasingType easing, uint32_t maskId)
  {
    uint32_t id = 0;
    for (const auto& temporary : temporaryEffects_) {
      const PostEffectInstance* effect = stack_.Find(temporary.first);
      if (effect && effect->name == name && effect->maskId == maskId) {
        id = temporary.first;
        break;
      }
    }
    if (id == 0) {
      id = AddEffect(name, param, maskId);
    }
    if (id == 0 || !SetEffectParam(id, param)) {
      return 0;
    }
    temporaryEffects_[id] = { duration, 0.0f, easing, param };
    return id;
  }

  void PostEffectManager::Update(float deltaTime)
  {
    for (auto it = temporaryEffects_.begin(); it != temporaryEffects_.end();) {
      TemporaryEffectInfo& info = it->second;
      info.elapsedTime += deltaTime;
      if (info.elapsedTime >= info.duration) {
        stack_.Remove(it->first);
        it = temporaryEffects_.erase(it);
        continue;
      }

      const float fadeFactor = 1.0f - ApplyEasing(info.elapsedTime / info.duration, info.easing);
      if (PostEffectInstance* effect = stack_.Find(it->first)) {
        effect->param = ApplyFadeToParam(info.baseParam, fadeFactor);
      }
      ++it;
    }
  }

  void PostEffectManager::CancelTemporaryEffect(uint32_t id)
  {
    if (temporaryEffects_.erase(id) > 0) {
      stack_.Remove(id);
    }
  }

  bool PostEffectManager::SetEffectParam(uint32_t id, const EffectParam& param)
  {
    PostEffectInstance* effect = stack_.Find(id);
    if (!effect || effect->param.index() != param.index()) {
      Logger::Log("PostEffect parameter mismatch (id %u)", id);
      return false;
    }
    effect->param = param;
    return true;
  }

  void PostEffectManager::SetDissolveMaskTex(const std::string& textureName)
  {
    if (!textureName.empty()) {
      dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureName);
    }
  }

  void PostEffectManager::SetDissolveBaseTex(const std::string& textureName)
  {
    if (!textureName.empty()) {
      dissolveBaseSrvIndex_ = TextureManager::GetInstance()->GetSRVIndex(textureName);
    }
  }

  void PostEffectManager::RegisterBuiltinEffects()
  {
    RegisterPostEffect("GrayScale", { .pixelShader = L"GrayScale.PS.hlsl" });
    RegisterPostEffect("Vignette", { .pixelShader = L"Vignette.PS.hlsl", .defaultParam = VignetteParam{ .power = 0.0f, .range = 20.0f } });
    RegisterPostEffect("RadialBlur", { .pixelShader = L"RadialBlur.PS.hlsl", .defaultParam = RadialBlurParam{ .center = { 0.5f, 0.5f }, .blurWidth = 0.0f, .sampleCount = 8 } });
    RegisterPostEffect("RGBSplit", { .pixelShader = L"RGBSplit.PS.hlsl", .defaultParam = RGBSplitParam{} });
    RegisterPostEffect("BWFilter", { .pixelShader = L"BWFilter.PS.hlsl", .defaultParam = BWFilterParam{ .threshold = 0.5f } });
    RegisterPostEffect("LuminanceBasedOutline", { .pixelShader = L"LuminanceBasedOutline.PS.hlsl", .defaultParam = LuminanceOutlineParam{ .outlineThickness = 1.0f } });

    RegisterPostEffect("DepthBasedOutline", {
      .pixelShader  = L"DepthBasedOutline.PS.hlsl",
      .defaultParam = DepthOutlineParam{ .outlineThickness = 1.0f },
      .usesDepth    = true,
      .setup        = [](const PostEffectFrame& frame, EffectParam& param, CommandContext::ShaderBinder&) {
        std::get<DepthOutlineParam>(param).projectionInverse = Mat4x4::Inverse(frame.camera->GetProjectionMatrix());
      },
    });

    RegisterPostEffect("Fog", {
      .pixelShader  = L"Fog.PS.hlsl",
      .defaultParam = FogParam{ .color = { 1.0f, 1.0f, 1.0f, 1.0f }, .density = 0.0f },
      .usesDepth    = true,
      .setup        = [](const PostEffectFrame& frame, EffectParam&, CommandContext::ShaderBinder& binder) {
        struct CameraConstants {
          float nearPlane;
          float farPlane;
        };
        binder.BindConstants("gCamera"_id, CameraConstants{ frame.camera->GetNearClip(), frame.camera->GetFarClip() });
      },
    });

    RegisterPostEffect("Bloom", { .defaultParam = BloomParam{ .intensity = 1.0f, .threshold = 1.0f, .sigma = 2.0f, .kernelSize = 10 }, .execute = ExecuteBloom });

    RegisterPostEffect("Dissolve", {
      .pixelShader  = L"Dissolve.PS.hlsl",
      .defaultParam = DissolveParam{ .edgeColor = { 0.0f, 0.0f, 0.0f, 1.0f } },
      .setup        = [this](const PostEffectFrame&, EffectParam&, CommandContext::ShaderBinder& binder) {
        // 登録はテクスチャの読み込みより前なので、未設定ならここでエンジン既定のテクスチャを引く
        TextureManager* textureManager = TextureManager::GetInstance();
        binder.BindSRV("gMaskTexture"_id, dissolveMaskSrvIndex_ != 0 ? dissolveMaskSrvIndex_ : textureManager->GetEngineDefaultSRVIndex("noise0.png"));
        binder.BindSRV("gBaseTexture"_id, dissolveBaseSrvIndex_ != 0 ? dissolveBaseSrvIndex_ : textureManager->GetEngineDefaultSRVIndex("black.png"));
      },
    });

    RegisterPostEffect("WhiteNoise", {
      .pixelShader  = L"WhiteNoise.PS.hlsl",
      .defaultParam = WhiteNoiseParam{},
      .setup        = [](const PostEffectFrame&, EffectParam& param, CommandContext::ShaderBinder&) {
        std::get<WhiteNoiseParam>(param).time = FrameTimer::GetInstance()->GetGameTime();
      },
    });

    RegisterPostEffect("HalfTone", {
      .pixelShader  = L"HalfTone.PS.hlsl",
      .defaultParam = HalfToneParam{ .dotSize = 8.0f, .contrast = 1.0f },
      .setup        = [](const PostEffectFrame& frame, EffectParam& param, CommandContext::ShaderBinder&) {
        std::get<HalfToneParam>(param).screenSize = { static_cast<float>(frame.width), static_cast<float>(frame.height) };
      },
    });

    RegisterPostEffect("GaussianBlur", { .defaultParam = GaussianBlurParam{ .sigma = 2.0f, .kernelSize = 9 }, .execute = ExecuteGaussianBlur });
  }

  //------------------------------- プロファイル -------------------------------//

  bool PostEffectManager::SaveProfile(const std::string& name) const
  {
    std::error_code ec;
    std::filesystem::create_directories(kProfileDirectory, ec);
    std::ofstream ofs(ProfilePath(name));
    if (!ofs) {
      Logger::Log("PostEffect profile save failed: %s", name.c_str());
      return false;
    }
    ofs << SerializeProfile() << std::endl;
    return true;
  }

  std::string PostEffectManager::SerializeProfile() const
  {
    json effects = json::array();
    for (const PostEffectInstance& effect : stack_.GetEffects()) {
      if (temporaryEffects_.contains(effect.id)) {
        continue;
      }
      json entry = { { "Name", effect.name } };
      std::visit([&entry]<class T>(const T& value) {
        if constexpr (SerializableParam<T>) {
          entry["Param"] = value;
        }
      }, effect.param);
      if (effect.maskId != 0) {
        entry["MaskId"] = effect.maskId;
      }
      if (!effect.enabled) {
        entry["Enabled"] = false;
      }
      effects.push_back(std::move(entry));
    }
    return json{ { "Effects", effects } }.dump(4);
  }

  void PostEffectManager::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
#ifdef _DEBUG
    if (profileTracker_.IsEdited()) {
      out.push_back(std::format("PostEffect Manager: {}", profileName_[0] != '\0' ? profileName_.data() : "(unnamed profile)"));
    }
#else
    (void)out;
#endif
  }

  bool PostEffectManager::LoadProfile(const std::string& name)
  {
    std::ifstream ifs(ProfilePath(name));
    if (!ifs) {
      Logger::Log("PostEffect profile not found: %s", name.c_str());
      return false;
    }
    if (!ApplyProfile(std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()))) {
      Logger::Log("PostEffect profile load failed: %s", name.c_str());
      return false;
    }

#ifdef _DEBUG
    if (profileTracker_.IsEdited()) {
      LogToConsole(std::format("PostEffect: unsaved profile changes were overwritten by loading '{}'", name), DebugUIManager::LogType::Warning);
    }
    profileTracker_.Reset();
#endif
    // Save の既定の保存先を、今読み込んだプロファイルにする
    std::ranges::fill(profileName_, '\0');
    name.copy(profileName_.data(), profileName_.size() - 1);
    return true;
  }

  bool PostEffectManager::ApplyProfile(const std::string& profile)
  {
    // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
    try {
      const json root = json::parse(profile);

      // 一時エフェクトはプロファイルに入らないので、時間切れまで末尾に残す
      std::vector<PostEffectInstance> temporaries;
      for (PostEffectInstance& effect : stack_.GetEffects()) {
        if (temporaryEffects_.contains(effect.id)) {
          temporaries.push_back(std::move(effect));
        }
      }
      stack_.Clear();

      for (const json& entry : root.at("Effects")) {
        const std::string           name       = entry.at("Name").get<std::string>();
        const PostEffectDefinition* definition = FindPostEffect(name);
        if (!definition) {
          continue;
        }
        EffectParam param = definition->defaultParam;
        if (const auto value = entry.find("Param"); value != entry.end()) {
          std::visit([&value]<class T>(T& typed) {
            if constexpr (SerializableParam<T>) {
              value->get_to(typed);
            }
          }, param);
        }
        const uint32_t id = stack_.Add(name, param, entry.value("MaskId", 0u));
        stack_.Find(id)->enabled = entry.value("Enabled", true);
      }

      std::ranges::move(temporaries, std::back_inserter(stack_.GetEffects()));
      return true;
    }
    catch (const json::exception& e) {
      Logger::Log("PostEffect profile apply failed: %s", e.what());
      return false;
    }
  }

  //------------------------------- 編集 UI -------------------------------//

#ifdef _DEBUG
  std::span<const ShortcutBinding<PostEffectManager>> PostEffectManager::Shortcuts()
  {
    static constexpr ShortcutBinding<PostEffectManager> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the profile (when a profile name is entered)", [](PostEffectManager& manager) {
        if (manager.profileName_[0] != '\0') {
          manager.SaveCurrentProfile();
        }
      } },
    };
    return kShortcuts;
  }

  bool PostEffectManager::SaveCurrentProfile()
  {
    const bool saved = SaveProfile(profileName_.data());
    LogToConsole(std::format("PostEffect profile {}: {}", saved ? "saved" : "save failed", profileName_.data()),
                 saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
    if (saved) {
      profileTracker_.MarkSaved();
    }
    return saved;
  }
#endif

  void PostEffectManager::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::Begin("PostEffect Manager");

    // UI を描く区間の前後で保存内容を比べて編集を検出する（この区間ではゲームのコードがスタックを変えない）
    profileTracker_.BeginSection();

    // 編集の比較は区間の最後なので、まだ編集していない区間の最初で保存する。パラメータは別タブで編集するので Save ボタンには付けない
    ProcessShortcuts(Shortcuts(), *this);

    if (ImGui::BeginTabBar("PostEffectTab")) {
      if (ImGui::BeginTabItem("PostEffect")) {
        DrawStackTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("PostEffectParam")) {
        DrawEffectParametersTab();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    profileTracker_.EndSection();
    ImGui::End();
#endif
  }

#ifdef _DEBUG
  void PostEffectManager::DrawStackTab()
  {
    auto loadProfile = [this](const std::string& name) {
      // 破棄を選んだ後なので、LoadProfile の上書きの警告は出さない
      profileTracker_.Reset();
      const bool loaded = LoadProfile(name);
      LogToConsole(std::format("PostEffect profile {}: {}", loaded ? "loaded" : "load failed", name),
                   loaded ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
    };

    ImGui::SeparatorText(profileTracker_.IsEdited() ? "Profile *" : "Profile");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##ProfileName", profileName_.data(), profileName_.size());
    ImGui::SameLine();
    const bool hasName = profileName_[0] != '\0';
    ImGui::BeginDisabled(!hasName);
    if (ImGui::Button("Save")) {
      SaveCurrentProfile();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("Load", "Select...")) {
      for (const std::string& name : ProfileNames()) {
        if (ImGui::Selectable(name.c_str())) {
          unsavedPrompt_.RunOrConfirm(profileTracker_.IsEdited(), std::format("Profile '{}' has unsaved changes.", profileName_.data()), "Load",
                                      [loadProfile, name] { loadProfile(name); });
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    profileTracker_.DrawButtons();

    std::function<bool()> save;
    if (hasName) {
      save = [this] { return SaveCurrentProfile(); };
    }
    unsavedPrompt_.Draw("Unsaved Changes##PostEffect", save);

    ImGui::Separator();
    ImGui::Columns(2, "EffectColumns", true);

    // === 左パネル: 登録済みのエフェクト（同じものを何個でも追加できる） ===
    ImGui::Text("Available Effects");
    ImGui::Separator();
    ImGui::BeginChild("AvailableList", ImVec2(0, 200), true);
    for (const std::string& name : GetPostEffectNames()) {
      if (FindPostEffect(name)->hidden) {
        continue;
      }
      if (ImGui::Selectable(name.c_str(), selectedAvailableEffect_ == name)) {
        selectedAvailableEffect_ = name;
      }
    }
    ImGui::EndChild();

    ImGui::BeginDisabled(selectedAvailableEffect_.empty());
    if (ImGui::Button("Add Effect", ImVec2(-1, 0))) {
      selectedActiveEffect_ = AddEffect(selectedAvailableEffect_);
    }
    ImGui::EndDisabled();

    ImGui::NextColumn();

    // === 右パネル: スタック（上から順に掛ける） ===
    std::vector<PostEffectInstance>& effects = stack_.GetEffects();
    ImGui::Text("Active Effects");
    ImGui::Separator();
    ImGui::BeginChild("ActiveList", ImVec2(0, 200), true);
    if (effects.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No effects active");
    }
    for (size_t i = 0; i < effects.size(); ++i) {
      const PostEffectInstance& effect = effects[i];
      std::string label = std::format("{}. {}", i + 1, effect.name);
      if (effect.maskId != 0) {
        label += std::format(" [mask {}]", effect.maskId);
      }
      if (temporaryEffects_.contains(effect.id)) {
        label += " (temporary)";
      }
      if (!effect.enabled) {
        label += " (off)";
      }
      if (ImGui::Selectable(std::format("{}##{}", label, effect.id).c_str(), selectedActiveEffect_ == effect.id)) {
        selectedActiveEffect_ = effect.id;
      }
    }
    ImGui::EndChild();

    // 削除で要素がずれても使えるよう、選択は添字で持つ
    const size_t selected     = std::ranges::find(effects, selectedActiveEffect_, &PostEffectInstance::id) - effects.begin();
    bool         hasSelection = selected < effects.size();

    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("Remove Effect", ImVec2(-1, 0))) {
      RemoveEffect(selectedActiveEffect_);
      selectedActiveEffect_ = 0;
      hasSelection = false;
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::BeginDisabled(!hasSelection || selected == 0);
    if (ImGui::Button("Move Up", ImVec2(80, 0))) {
      std::swap(effects[selected], effects[selected - 1]);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSelection || selected + 1 >= effects.size());
    if (ImGui::Button("Move Down", ImVec2(-1, 0))) {
      std::swap(effects[selected], effects[selected + 1]);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (ImGui::Button("Clear All", ImVec2(80, 0))) {
      ClearEffects();
      selectedActiveEffect_ = 0;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(effects.empty());
    if (ImGui::Button("Reverse Order", ImVec2(-1, 0))) {
      std::ranges::reverse(effects);
    }
    ImGui::EndDisabled();

    ImGui::Columns(1);
  }

  void PostEffectManager::DrawEffectParametersTab()
  {
    std::vector<PostEffectInstance>& effects = stack_.GetEffects();
    if (effects.empty()) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active effects to configure");
      ImGui::TextWrapped("Switch to the 'PostEffect' tab to enable effects first.");
      return;
    }

    for (size_t i = 0; i < effects.size(); ++i) {
      PostEffectInstance& effect = effects[i];
      ImGui::PushID(static_cast<int>(effect.id));
      if (ImGui::CollapsingHeader(std::format("[{}] {}", i + 1, effect.name).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &effect.enabled);
        int maskId = static_cast<int>(effect.maskId);
        if (ImGui::InputInt("Mask ID", &maskId)) {
          effect.maskId = static_cast<uint32_t>(std::clamp(maskId, 0, 255));
        }
        ImGui::SetItemTooltip("0 = whole screen. Otherwise only objects with the same Object3d::SetMaskId");
        std::visit([](auto& param) { DrawParam(param); }, effect.param);
      }
      ImGui::PopID();
    }
  }
#endif

  //------------------------------- 一時エフェクトのフェード -------------------------------//

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
