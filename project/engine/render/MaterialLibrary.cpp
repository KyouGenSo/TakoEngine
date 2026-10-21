#include "MaterialLibrary.h"

#include <algorithm>
#include <d3d12shader.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

#include "DX12Basic.h"
#include "EnginePaths.h"
#include "Logger.h"
#include "Material.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "StringUtility.h"
#include "TextureManager.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

    const std::filesystem::path kShaderDirectory   = "resources/Shaders";
    const std::filesystem::path kMaterialDirectory = "resources/Materials";
    constexpr std::string_view  kMaterialExtension = ".mat.json";
    constexpr std::string_view  kShaderExtension   = ".shader.json";

    std::filesystem::path MaterialPath(const std::string& name)
    {
      return kMaterialDirectory / (name + std::string(kMaterialExtension));
    }

    std::optional<std::string> ReadText(const std::filesystem::path& path)
    {
      std::ifstream ifs(path);
      if (!ifs) {
        return std::nullopt;
      }
      return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    std::filesystem::file_time_type LastWriteTime(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto time = std::filesystem::last_write_time(path, error);
      return error ? std::filesystem::file_time_type{} : time;
    }

    bool Declares(const Shader& shader, const char* name)
    {
      D3D12_SHADER_INPUT_BIND_DESC bind{};
      return shader.GetReflection() && SUCCEEDED(shader.GetReflection()->GetResourceBindingDescByName(name, &bind));
    }

    template<class T, size_t N>
    std::optional<T> FindByName(const char* const (&names)[N], const std::string& name)
    {
      for (size_t i = 0; i < N; ++i) {
        if (name == names[i]) {
          return static_cast<T>(i);
        }
      }
      return std::nullopt;
    }

    /// <summary>
    /// JSON に書かれた描画設定だけを読む（書いていない項目は値なし）
    /// </summary>
    RenderStateOverride ReadRenderStateOverride(const json& source)
    {
      RenderStateOverride state;
      state.blend = FindByName<BlendMode>(kBlendModeNames, source.value("Blend", std::string()));
      state.cull = FindByName<CullMode>(kCullModeNames, source.value("Cull", std::string()));
      if (const auto depthTest = source.find("DepthTest"); depthTest != source.end()) {
        state.depthTest = depthTest->get<bool>();
      }
      if (const auto depthWrite = source.find("DepthWrite"); depthWrite != source.end()) {
        state.depthWrite = depthWrite->get<bool>();
      }
      if (const auto func = FindByName<int>(kDepthFuncNames, source.value("DepthFunc", std::string()))) {
        state.depthFunc = static_cast<D3D12_COMPARISON_FUNC>(*func + 1);
      }
      return state;
    }

    /// <summary>
    /// 値のある描画設定だけを JSON に書く
    /// </summary>
    void WriteRenderStateOverride(const RenderStateOverride& state, json& target)
    {
      if (state.blend) {
        target["Blend"] = kBlendModeNames[static_cast<size_t>(*state.blend)];
      }
      if (state.cull) {
        target["Cull"] = kCullModeNames[static_cast<size_t>(*state.cull)];
      }
      if (state.depthTest) {
        target["DepthTest"] = *state.depthTest;
      }
      if (state.depthWrite) {
        target["DepthWrite"] = *state.depthWrite;
      }
      if (state.depthFunc) {
        target["DepthFunc"] = kDepthFuncNames[*state.depthFunc - 1];
      }
    }

  } // anonymous namespace

  std::unique_ptr<MaterialLibrary> MaterialLibrary::instance_ = nullptr;

  MaterialLibrary* MaterialLibrary::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<MaterialLibrary>(Token{});
    }
    return instance_.get();
  }

  void MaterialLibrary::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;
    for (const std::string& path : GetShaderFiles()) {
      shaderFileTimes_[path] = LastWriteTime(path);
      LoadShaderFile(path);
    }
  }

  void MaterialLibrary::Finalize()
  {
    instance_.reset();
  }

  void MaterialLibrary::Update()
  {
#ifdef _DEBUG
    // 追加・保存された .shader.json を読み直す
    const auto now = std::chrono::steady_clock::now();
    if (now - lastShaderCheck_ >= kShaderCheckInterval) {
      lastShaderCheck_ = now;
      for (const std::string& path : GetShaderFiles()) {
        const std::filesystem::file_time_type time = LastWriteTime(path);
        const auto [it, added] = shaderFileTimes_.try_emplace(path, time);
        if (added || it->second != time) {
          it->second = time;
          LoadShaderFile(path);
        }
      }
    }

    // PS が再コンパイルされたポストエフェクトは、パラメータの並びを作り直すために登録し直す
    std::vector<std::string> recompiled;
    for (auto& [name, info] : shaders_) {
      if (info.postEffectPs && info.postEffectPs->GetVersion() != info.postEffectVersion) {
        info.postEffectVersion = info.postEffectPs->GetVersion();
        recompiled.push_back(info.path);
      }
    }
    for (const std::string& path : recompiled) {
      LoadShaderFile(path);
    }

    for (auto& [name, asset] : assets_) {
      const std::shared_ptr<const MaterialPropertyLayout> layout = GetLayout(*asset.material->GetShader());
      if (layout == asset.layout) {
        continue;
      }
      // 新しく増えた値は初期値から始め、残った値は名前で移す（同じ大きさでもオフセットはずれ得る）
      const json values = asset.layout->SerializeValues(asset.material->GetConstantBytes());
      asset.layout = layout;
      ResetValues(asset, values);
      FitTextures(asset);
    }
#endif
  }

  std::shared_ptr<Material> MaterialLibrary::Load(const std::string& name)
  {
    if (std::shared_ptr<Material> loaded = Find(name)) {
      return loaded;
    }
    const std::optional<std::string> text = ReadText(MaterialPath(name));
    if (!text) {
      Logger::Log("Material not found: %s", name.c_str());
      return nullptr;
    }

    // 書かれたシェーダーで作ってから、値と描画設定を当てはめる
    std::string shaderName;
    try {
      shaderName = json::parse(*text).at("Shader").get<std::string>();
    }
    catch (const json::exception& e) {
      Logger::Log("Material load failed: %s (%s)", name.c_str(), e.what());
      return nullptr;
    }
    if (!Create(name, shaderName) || !Apply(name, *text)) {
      assets_.erase(name);
      return nullptr;
    }
    return assets_.at(name).material;
  }

  std::shared_ptr<Material> MaterialLibrary::Create(const std::string& name, const std::string& shaderName)
  {
    const ShaderAsset* shader = Renderer::GetInstance()->FindShader(shaderName);
    if (!shader) {
      Logger::Log("Material shader not registered: %s", shaderName.c_str());
      return nullptr;
    }

    Asset& asset = assets_[name];
    if (!asset.material) {
      asset.material = std::make_shared<Material>(dx12_, shader);
    }
    else {
      asset.material->SetShader(shader);
    }
    asset.layout = GetLayout(*shader);
    ResetValues(asset, json::object());
    FitTextures(asset);
    return asset.material;
  }

  bool MaterialLibrary::Save(const std::string& name) const
  {
    if (!Find(name)) {
      return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(kMaterialDirectory, ec);
    std::ofstream ofs(MaterialPath(name));
    if (!ofs) {
      Logger::Log("Material save failed: %s", name.c_str());
      return false;
    }
    ofs << Serialize(name) << std::endl;
    return true;
  }

  std::string MaterialLibrary::Serialize(const std::string& name) const
  {
    const auto it = assets_.find(name);
    if (it == assets_.end()) {
      return {};
    }
    const Asset&    asset    = it->second;
    const Material& material = *asset.material;
    json root = {
      { "Shader",   material.GetShader()->GetName() },
      { "Values",   asset.layout->SerializeValues(material.GetConstantBytes()) },
      { "Textures", asset.textures },
    };
    // 上書きした描画設定だけを書く（書いていない項目はシェーダーの値に追従する）
    WriteRenderStateOverride(material.GetStateOverride(), root);
    if (const std::optional<int32_t> queue = material.GetQueueOverride()) {
      root["Queue"] = *queue;
    }
    return root.dump(4);
  }

  bool MaterialLibrary::Apply(const std::string& name, const std::string& text)
  {
    const auto it = assets_.find(name);
    if (it == assets_.end()) {
      return false;
    }
    Asset& asset = it->second;

    try {
      const json         root   = json::parse(text);
      const ShaderAsset* shader = Renderer::GetInstance()->FindShader(root.at("Shader").get<std::string>());
      if (!shader) {
        Logger::Log("Material shader not registered: %s", root.at("Shader").get<std::string>().c_str());
        return false;
      }

      // シェーダーを替えても、同じ名前の値は引き継ぐ
      Material&  material = *asset.material;
      const json previous = asset.layout->SerializeValues(material.GetConstantBytes());
      if (material.GetShader() != shader) {
        material.SetShader(shader);
      }
      asset.layout = GetLayout(*shader);
      ResetValues(asset, previous);
      asset.layout->DeserializeValues(root.value("Values", json::object()), material.GetConstantBytes(asset.layout->constantSize));

      // 書いていない描画設定はシェーダーの値を使う
      material.SetStateOverride(ReadRenderStateOverride(root));
      const auto queue = root.find("Queue");
      material.SetQueueOverride(queue != root.end() ? std::optional<int32_t>(queue->get<int32_t>()) : std::nullopt);

      asset.textures = root.value("Textures", std::map<std::string, std::string>{});
      FitTextures(asset);
      return true;
    }
    catch (const json::exception& e) {
      Logger::Log("Material apply failed: %s (%s)", name.c_str(), e.what());
      return false;
    }
  }

  bool MaterialLibrary::DrawValues(const std::string& name)
  {
    const auto it = assets_.find(name);
    if (it == assets_.end()) {
      return false;
    }
    Asset& asset = it->second;
    return asset.layout->DrawImGui(asset.material->GetConstantBytes(asset.layout->constantSize));
  }

  std::shared_ptr<const MaterialPropertyLayout> MaterialLibrary::GetLayout(const ShaderAsset& shader)
  {
    const GraphicsPipelineDesc* forward = shader.GetPass(ShaderPass::Forward);
    const Shader*               ps      = forward ? forward->ps : nullptr;
    ShaderInfo&                 info    = shaders_[shader.GetName()];
    // ホットリロードで PS のバージョンが上がったら作り直す
    const uint32_t version = ps ? ps->GetVersion() : 0;
    if (!info.layout || info.version != version) {
      info.layout = MaterialPropertyLayout::Reflect(ps ? ps->GetReflection() : nullptr, "gMaterial", info.ranges);
      info.version = version;
    }
    return info.layout;
  }

  uint32_t MaterialLibrary::ResolveTexture(const std::string& file)
  {
    TextureManager* textureManager = TextureManager::GetInstance();
    if (file.empty()) {
      return textureManager->GetEngineDefaultSRVIndex("white.png");
    }
    // 無いファイルを読もうとすると TextureManager が止まるので、先に確かめる
    if (!textureManager->IsLoaded(file) && !std::filesystem::exists(textureManager->GetFilePath(file))) {
      Logger::Log("Texture not found: %s", file.c_str());
      return textureManager->GetEngineDefaultSRVIndex("white.png");
    }
    textureManager->LoadTexture(file);
    return textureManager->GetSRVIndex(file);
  }

  std::vector<std::string> MaterialLibrary::GetMaterialFileNames() const
  {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kMaterialDirectory, ec)) {
      const std::string fileName = entry.path().filename().string();
      if (fileName.ends_with(kMaterialExtension)) {
        names.push_back(fileName.substr(0, fileName.size() - kMaterialExtension.size()));
      }
    }
    return names;
  }

  std::vector<std::string> MaterialLibrary::GetShaderFiles() const
  {
    std::vector<std::string> files;
    for (const std::filesystem::path& directory : { std::filesystem::path(EnginePaths::kEngineShaders), kShaderDirectory }) {
      std::vector<std::string> found;
      std::error_code ec;
      for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (entry.path().filename().string().ends_with(kShaderExtension)) {
          found.push_back(entry.path().generic_string());
        }
      }
      std::ranges::sort(found);
      files.insert(files.end(), found.begin(), found.end());
    }
    return files;
  }

  std::shared_ptr<Material> MaterialLibrary::Find(const std::string& name) const
  {
    const auto it = assets_.find(name);
    return it != assets_.end() ? it->second.material : nullptr;
  }

  bool MaterialLibrary::LoadShaderFile(const std::string& path)
  {
    const std::optional<std::string> text = ReadText(path);
    return text && ApplyShaderText(path, *text);
  }

  bool MaterialLibrary::ApplyShaderText(const std::string& path, const std::string& text)
  {
    try {
      const json        root    = json::parse(text);
      const std::string name    = root.at("Name").get<std::string>();
      ShaderLibrary&    library = dx12_->GetShaderLibrary();
      const auto        forward = root.find("Forward");
      const auto        post    = root.find("PostEffect");

      // 先に全てコンパイルし、失敗したら今の登録を残す
      GraphicsPipelineDesc pipeline;
      if (forward != root.end()) {
        pipeline.vs = library.TryGet({ StringUtility::ConvertString(forward->value("VS", std::string("Object3D.VS.hlsl"))), ShaderStage::Vertex });
        pipeline.ps = library.TryGet({ StringUtility::ConvertString(forward->at("PS").get<std::string>()), ShaderStage::Pixel });
      }
      std::wstring  postPsFile;
      const Shader* postPs = nullptr;
      if (post != root.end()) {
        postPsFile = StringUtility::ConvertString(post->at("PS").get<std::string>());
        postPs = library.TryGet({ postPsFile, ShaderStage::Pixel });
      }
      if ((forward != root.end() && (!pipeline.vs || !pipeline.ps)) || (post != root.end() && !postPs)) {
        Logger::Log("Shader asset skipped (compile failed): %s", path.c_str());
        return false;
      }

      ShaderInfo& info = shaders_[name];
      info.path = path;
      info.ranges.clear();
      // value() の戻り値は一時オブジェクトなので、items() で回す前に受けておく
      const json properties = root.value("Properties", json::object());
      for (const auto& [property, range] : properties.items()) {
        if (range.is_array() && range.size() == 2) {
          info.ranges[property] = { range[0].get<float>(), range[1].get<float>() };
        }
      }
      info.defaults = root.value("Defaults", json::object());
      info.layout.reset();

      // 描画物のシェーダー
      if (forward != root.end()) {
        pipeline.vertexLayout = VertexLayout::Mesh;
        pipeline.renderState.blend = BlendMode::Alpha;
        pipeline.renderState = ReadRenderStateOverride(*forward).ApplyTo(pipeline.renderState);

        Renderer*    renderer = Renderer::GetInstance();
        ShaderAsset& shader   = renderer->RegisterShader(name);
        // 読み直しで Fallback を外したときに、前の影・マスクのパスを残さない
        shader = ShaderAsset(name);
        // 色を決めないパス（影・マスク）は Fallback のシェーダーのものを使う
        if (const ShaderAsset* fallback = renderer->FindShader(root.value("Fallback", std::string()))) {
          for (ShaderPass pass : { ShaderPass::ShadowCaster, ShaderPass::Mask }) {
            for (bool instanced : { false, true }) {
              if (const GraphicsPipelineDesc* source = fallback->GetPass(pass, instanced)) {
                shader.SetPass(pass, *source, instanced);
              }
            }
          }
        }
        shader.SetPass(ShaderPass::Forward, pipeline);
        shader.SetDefaultQueue(root.value("Queue", ShaderAsset::kQueueGeometry));
      }

      // ポストエフェクト
      if (post != root.end()) {
        PropertyBlock block{ MaterialPropertyLayout::Reflect(postPs->GetReflection(), "gParam", info.ranges), {} };
        block.bytes.resize(block.layout->constantSize);
        block.layout->DeserializeValues(info.defaults, block.bytes);

        // gTexture（入力の画面）以外のテクスチャには Textures のファイルを渡す
        std::vector<std::pair<ShaderParamId, uint32_t>> textures;
        const json files = post->value("Textures", json::object());
        for (const std::string& slot : block.layout->textures) {
          if (slot != "gTexture") {
            textures.emplace_back(MakeShaderParamId(slot), ResolveTexture(files.value(slot, std::string())));
          }
        }
        PostEffectDefinition definition{ .pixelShader = postPsFile, .defaultParam = std::move(block), .usesDepth = Declares(*postPs, "gDepthTexture") };
        if (!textures.empty()) {
          definition.setup = [textures](const PostEffectFrame&, EffectParam&, CommandContext::ShaderBinder& binder) {
            for (const auto& [id, srvIndex] : textures) {
              binder.BindSRV(id, srvIndex);
            }
          };
        }
        RegisterPostEffect(name, std::move(definition));
        info.postEffectPs = postPs;
        info.postEffectVersion = postPs->GetVersion();
      }
      return true;
    }
    catch (const json::exception& e) {
      Logger::Log("Shader asset load failed: %s (%s)", path.c_str(), e.what());
      return false;
    }
  }

  void MaterialLibrary::ResetValues(Asset& asset, const json& previous)
  {
    const std::span<uint8_t> bytes = asset.material->GetConstantBytes(asset.layout->constantSize);
    std::ranges::fill(bytes, uint8_t{ 0 });
    asset.layout->DeserializeValues(shaders_[asset.material->GetShader()->GetName()].defaults, bytes);
    asset.layout->DeserializeValues(previous, bytes);
  }

  void MaterialLibrary::FitTextures(Asset& asset) const
  {
    Material&                       material = *asset.material;
    const std::vector<std::string>& slots    = asset.layout->textures;
    std::erase_if(asset.textures, [&](const auto& texture) {
      if (std::ranges::find(slots, texture.first) != slots.end()) {
        return false;
      }
      material.SetTexture(MakeShaderParamId(texture.first), 0);
      return true;
    });

    for (const std::string& slot : slots) {
      const auto file = asset.textures.find(slot);
      material.SetTexture(MakeShaderParamId(slot), ResolveTexture(file != asset.textures.end() ? file->second : std::string()));
    }
  }

} // namespace Tako
