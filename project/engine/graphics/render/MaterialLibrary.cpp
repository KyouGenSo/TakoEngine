#include "MaterialLibrary.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>

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

    /// <summary>
    /// .shader.json のポストエフェクト（値を PropertyBlock で持つ定義）の既定値。組み込みの型付きエフェクトや未登録なら nullptr
    /// </summary>
    const PropertyBlock* FindPostEffectDefaults(const std::string& shader)
    {
      const PostEffectDefinition* definition = FindPostEffect(shader);
      return definition ? std::get_if<PropertyBlock>(&definition->defaultParam) : nullptr;
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

    /// <summary>
    /// .shader.json のポストエフェクトの 1 パス（読み込み時に名前を解決したもの）
    /// </summary>
    struct PostEffectPass {
      const Shader*                                   ps        = nullptr;
      std::vector<std::pair<ShaderParamId, int32_t>>  inputs;                           ///< スロット → 読む画像（-1 はエフェクトの入力、0 以上はそのパスの出力）
      std::vector<std::pair<ShaderParamId, uint32_t>> textures;                         ///< スロット → Textures のファイルの SRV
      float                                           scale     = 1.0f;                 ///< エフェクトの入力に対する大きさ。最後のパスはエフェクトの出力へ書くので使わない
      DXGI_FORMAT                                     format    = DXGI_FORMAT_UNKNOWN;  ///< 一時 RT のフォーマット。UNKNOWN はエフェクトの入力と同じ。最後のパスでは使わない
      bool                                            usesDepth = false;
    };

    /// <summary>
    /// part の値が全て whole の同じ名前・同じ位置・同じ型にあるか（whole の値の組を part のパスへそのまま渡せるか）
    /// </summary>
    bool ReadsSameValues(const MaterialPropertyLayout& whole, const MaterialPropertyLayout& part)
    {
      return part.constantSize <= whole.constantSize && std::ranges::all_of(part.properties, [&whole](const MaterialProperty& property) {
        const MaterialProperty* same = whole.Find(property.id);
        return same && same->offset == property.offset && same->type == property.type && same->components == property.components;
      });
    }

    /// <summary>
    /// "Forward"（パスの配列か 1 つのパス）をコンパイルする。1 番目のパスの gMaterial とテクスチャがマテリアルの値になる
    /// </summary>
    /// <returns>読めなければ理由（読めたら空）</returns>
    std::string BuildForwardPasses(ShaderLibrary& library, const json& forward, std::vector<GraphicsPipelineDesc>& passes)
    {
      const json passList = forward.is_array() ? forward : json::array({ forward });
      if (passList.empty()) {
        return "Forward needs at least one pass";
      }
      std::shared_ptr<MaterialPropertyLayout> values;
      for (size_t i = 0; i < passList.size(); ++i) {
        const json&           entry    = passList[i];
        const std::string     label    = std::format("Pass {}", i + 1);
        GraphicsPipelineDesc& pipeline = passes.emplace_back();
        pipeline.vs = library.TryGet({ StringUtility::ConvertString(entry.value("VS", std::string("Object3D.VS.hlsl"))), ShaderStage::Vertex });
        pipeline.ps = library.TryGet({ StringUtility::ConvertString(entry.at("PS").get<std::string>()), ShaderStage::Pixel,
                                       StringUtility::ConvertString(entry.value("Entry", std::string("main"))) });
        if (!pipeline.vs || !pipeline.ps) {
          return label + ": compile failed";
        }
        pipeline.vertexLayout = VertexLayout::Mesh;
        pipeline.renderState.blend = BlendMode::Alpha;
        pipeline.renderState = ReadRenderStateOverride(entry).ApplyTo(pipeline.renderState);

        if (i == 0) {
          values = MaterialPropertyLayout::Reflect(pipeline.ps->GetReflection(), "gMaterial", {});
          continue;
        }
        // 他のパスはマテリアルが持つ値とテクスチャだけを読める
        for (const Shader* shader : { pipeline.vs, pipeline.ps }) {
          const std::shared_ptr<MaterialPropertyLayout> reflected = MaterialPropertyLayout::Reflect(shader->GetReflection(), "gMaterial", {});
          if (!ReadsSameValues(*values, *reflected)) {
            return label + ": gMaterial has values that differ from the first pass";
          }
          for (const std::string& texture : reflected->textures) {
            if (std::ranges::find(values->textures, texture) == values->textures.end()) {
              return std::format("{}: {} is not declared by the first pass", label, texture);
            }
          }
        }
      }
      return {};
    }

    /// <summary>
    /// "PostEffect" の Passes（無ければ PostEffect 自体を 1 パスとする）をコンパイルし、入出力の名前を解決する
    /// </summary>
    /// <param name="layout">全パスで共通の gParam の並びと、Textures で渡すスロットを受け取る</param>
    /// <returns>読めなければ理由（読めたら空）</returns>
    std::string BuildPostEffectPasses(ShaderLibrary& library, const json& post, const std::map<std::string, std::pair<float, float>>& ranges,
                                      std::vector<PostEffectPass>& passes, std::shared_ptr<MaterialPropertyLayout>& layout)
    {
      const json passList = post.contains("Passes") ? post.at("Passes") : json::array({ post });
      if (!passList.is_array() || passList.empty()) {
        return "PostEffect needs PS or Passes";
      }
      const json               files = post.value("Textures", json::object());
      std::vector<std::string> outputs;  ///< パスごとの Output の名前
      layout = std::make_shared<MaterialPropertyLayout>();
      bool hasParam = false;

      for (size_t i = 0; i < passList.size(); ++i) {
        const json&       entry = passList[i];
        const bool        last  = i + 1 == passList.size();
        const std::string label = std::format("Pass {}", i + 1);
        PostEffectPass&   pass  = passes.emplace_back();

        const std::wstring file = StringUtility::ConvertString(entry.at("PS").get<std::string>());
        pass.ps = library.TryGet({ file, ShaderStage::Pixel, StringUtility::ConvertString(entry.value("Entry", std::string("main"))) });
        if (!pass.ps) {
          return label + ": compile failed";
        }
        pass.usesDepth = pass.ps->Declares("gDepthTexture");
        if (!last) {
          pass.scale = entry.value("Scale", 1.0f);
          if (!(pass.scale > 0.0f)) {
            return label + ": Scale must be greater than 0";
          }
          if (const std::string formatName = entry.value("Format", std::string()); !formatName.empty()) {
            const auto known = std::ranges::find_if(kPostEffectFormats, [&formatName](const auto& candidate) { return formatName == candidate.first; });
            if (known == std::end(kPostEffectFormats)) {
              return std::format("{}: unknown Format {}", label, formatName);
            }
            pass.format = known->second;
          }
        }

        // 値の組は 1 つなので、他のパスはそれと同じ位置の値だけを読める（Unity の SRP Batcher が全 Pass に同じ定数を求めるのと同じ考え）
        const std::shared_ptr<MaterialPropertyLayout> reflected = MaterialPropertyLayout::Reflect(pass.ps->GetReflection(), "gParam", ranges);
        if (pass.ps->Declares("gParam")) {
          if (!hasParam) {
            layout->constantSize = reflected->constantSize;
            layout->properties   = reflected->properties;
            hasParam = true;
          }
          else if (!ReadsSameValues(*layout, *reflected)) {
            return label + ": gParam has values that differ from the first pass that declares it";
          }
        }

        // 書かない gTexture はエフェクトの入力、Inputs にも gTexture にも当たらないスロットは Textures のファイル
        const json inputs = entry.value("Inputs", json::object());
        for (const auto& [slot, source] : inputs.items()) {
          if (std::ranges::find(reflected->textures, slot) == reflected->textures.end()) {
            return std::format("{}: the shader does not declare {}", label, slot);
          }
        }
        for (const std::string& slot : reflected->textures) {
          const ShaderParamId id     = MakeShaderParamId(slot);
          const auto          source = inputs.find(slot);
          const std::string   name   = source != inputs.end() ? source->get<std::string>() : (slot == "gTexture" ? "Source" : "");
          if (name.empty()) {
            pass.textures.emplace_back(id, MaterialLibrary::ResolveTexture(files.value(slot, std::string())));
            if (std::ranges::find(layout->textures, slot) == layout->textures.end()) {
              layout->textures.push_back(slot);
            }
            continue;
          }
          if (name == "Source") {
            pass.inputs.emplace_back(id, -1);
            continue;
          }
          const auto produced = std::ranges::find(outputs, name);
          if (produced == outputs.end()) {
            return std::format("{}: {} is not an Output of an earlier pass", label, name);
          }
          pass.inputs.emplace_back(id, static_cast<int32_t>(produced - outputs.begin()));
        }

        const std::string output = last ? std::string() : entry.value("Output", std::string());
        if (!last && output.empty()) {
          return label + ": Output is required except on the last pass";
        }
        if (output == "Source" || (!output.empty() && std::ranges::find(outputs, output) != outputs.end())) {
          return std::format("{}: Output {} is already used", label, output);
        }
        outputs.push_back(output);
      }
      return {};
    }

    /// <summary>
    /// passes を順に描く。途中のパスは縮小した一時 RT へ書き、最後のパスは output へ書く
    /// ponytail: 途中の RT はエフェクトの最後までまとめて借りる。VRAM が厳しければ最後に読むパスの後で返す
    /// </summary>
    void ExecutePostEffectPasses(std::span<const PostEffectPass> passes, const PostEffectFrame& frame, const EffectParam& param, RenderTexture& input, RenderTexture& output)
    {
      RenderGraph& graph = *frame.graph;

      // 定数は 1 回だけ書き、全パスで同じものを渡す
      const PropertyBlock*      block     = std::get_if<PropertyBlock>(&param);
      D3D12_GPU_VIRTUAL_ADDRESS constants = 0;
      if (block && !block->bytes.empty()) {
        const UploadAllocator::Allocation allocation = frame.dx12->GetUploadAllocator().Allocate(block->bytes.size());
        std::memcpy(allocation.cpu, block->bytes.data(), block->bytes.size());
        constants = allocation.gpu;
      }

      std::vector<RenderTexture*> written(passes.size(), nullptr);
      const auto source = [&](int32_t index) -> RenderTexture& { return index < 0 ? input : *written[index]; };
      std::vector<RenderGraph::Access> accesses;

      for (size_t i = 0; i < passes.size(); ++i) {
        const PostEffectPass& pass = passes[i];
        const auto scaled = [&](uint32_t size) { return (std::max)(1u, static_cast<uint32_t>(static_cast<float>(size) * pass.scale)); };
        const DXGI_FORMAT format = pass.format != DXGI_FORMAT_UNKNOWN ? pass.format : input.format;
        RenderTexture& target = i + 1 == passes.size() ? output : graph.Acquire({ scaled(input.width), scaled(input.height), format });
        written[i] = &target;

        accesses.clear();
        for (const auto& [slot, index] : pass.inputs) {
          accesses.push_back(RenderGraph::Read(source(index).resource));
        }
        if (pass.usesDepth) {
          accesses.push_back(RenderGraph::Read(*frame.depth));
        }
        accesses.push_back(RenderGraph::Write(target.resource));

        graph.AddPass(accesses, [&] {
          CommandContext::ShaderBinder binder = BeginFullScreen(frame, *pass.ps, target);
          for (const auto& [slot, index] : pass.inputs) {
            binder.BindSRV(slot, source(index).srvIndex);
          }
          // マテリアルが同じスロットのテクスチャを持てば、.shader.json の Textures の代わりに渡す
          for (const auto& [slot, srvIndex] : pass.textures) {
            uint32_t srv = srvIndex;
            if (block) {
              if (const auto replaced = std::ranges::find(block->textures, slot, &std::pair<ShaderParamId, uint32_t>::first); replaced != block->textures.end()) {
                srv = replaced->second;
              }
            }
            binder.BindSRV(slot, srv);
          }
          if (pass.usesDepth) {
            binder.BindSRV("gDepthTexture"_id, frame.depthSrv);
          }
          // 宣言していないシェーダーには何もしない
          if (frame.view != 0) {
            binder.BindCBV("gView"_id, frame.view);
          }
          if (constants != 0) {
            binder.BindCBV("gParam"_id, constants);
          }
          DrawFullScreen(frame);
        });
      }

      for (size_t i = 0; i + 1 < passes.size(); ++i) {
        graph.Release(*written[i]);
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

    // PostEffect Editor の一覧に出すため、ポストエフェクトのマテリアルだけ先に読む（描画物のマテリアルは使うときに読む）
    // ponytail: 起動後に足したファイルやシェーダーを直してから読めるようになったものは、Material Editor で開くまで一覧に出ない
    for (const std::string& name : GetMaterialFileNames()) {
      const std::optional<std::string> text = ReadText(MaterialPath(name));
      if (!text) {
        continue;
      }
      const json root = json::parse(*text, nullptr, false);
      if (root.is_object() && root.contains("Shader") && root["Shader"].is_string() && FindPostEffectDefaults(root["Shader"].get<std::string>())) {
        LoadAsset(name);
      }
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
      const std::vector<std::string> files = GetShaderFiles();
      for (const std::string& path : files) {
        const std::filesystem::file_time_type time = LastWriteTime(path);
        const auto [it, added] = shaderFileTimes_.try_emplace(path, time);
        if (added || it->second != time) {
          it->second = time;
          LoadShaderFile(path);
        }
      }
      // 消えたファイルのポストエフェクトを外す（描画物のシェーダーはマテリアルが指しているので残す）
      for (auto it = shaderFileTimes_.begin(); it != shaderFileTimes_.end();) {
        if (std::ranges::find(files, it->first) != files.end()) {
          ++it;
          continue;
        }
        UnregisterPostEffects(it->first);
        it = shaderFileTimes_.erase(it);
      }
    }

    // PS が再コンパイルされたポストエフェクトは、パラメータの並びを作り直すために登録し直す
    std::vector<std::string> recompiled;
    for (auto& [name, info] : shaders_) {
      bool changed = false;
      for (auto& [ps, version] : info.postEffectShaders) {
        changed |= ps->GetVersion() != version;
        version = ps->GetVersion();
      }
      if (changed) {
        recompiled.push_back(info.path);
      }
    }
    for (const std::string& path : recompiled) {
      LoadShaderFile(path);
    }

    for (auto& [name, asset] : assets_) {
      // ポストエフェクトのマテリアルは値を名前で持つので移さなくてよい
      if (!asset.material) {
        continue;
      }
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
    if (!LoadAsset(name)) {
      return nullptr;
    }
    const Asset& asset = assets_.at(name);
    if (!asset.material) {
      Logger::Log("Material %s is for post effects (add it in PostEffect Editor)", name.c_str());
    }
    return asset.material;
  }

  std::shared_ptr<const PostEffectMaterial> MaterialLibrary::LoadPostEffect(const std::string& name)
  {
    return LoadAsset(name) ? assets_.at(name).post : nullptr;
  }

  bool MaterialLibrary::Create(const std::string& name, const std::string& shaderName)
  {
    // 用途はシェーダーで決まる（UE の Material Domain と同じ）
    if (FindPostEffectDefaults(shaderName)) {
      Asset& asset = assets_[name];
      asset.material.reset();
      asset.layout.reset();
      if (!asset.post) {
        asset.post = std::make_shared<PostEffectMaterial>();
      }
      asset.post->name   = name;
      asset.post->shader = shaderName;
      asset.post->values.Clear();
      asset.textures.clear();
      return true;
    }

    const ShaderAsset* shader = Renderer::GetInstance()->FindShader(shaderName);
    if (!shader) {
      Logger::Log("Material shader not registered: %s", shaderName.c_str());
      return false;
    }

    Asset& asset = assets_[name];
    asset.post.reset();
    if (!asset.material) {
      asset.material = std::make_shared<Material>(dx12_, shader);
    }
    else {
      asset.material->SetShader(shader);
    }
    asset.layout = GetLayout(*shader);
    ResetValues(asset, json::object());
    FitTextures(asset);
    return true;
  }

  bool MaterialLibrary::Save(const std::string& name) const
  {
    if (!assets_.contains(name)) {
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
    const Asset& asset = it->second;
    if (asset.post) {
      // 描画物のマテリアルと同じく全ての値を書く（シェーダーの Defaults を後で変えても、保存したマテリアルは変わらない）
      json values = json::object();
      if (const PropertyBlock* defaults = FindPostEffectDefaults(asset.post->shader)) {
        std::vector<uint8_t> bytes = defaults->bytes;
        asset.post->values.Apply(*defaults->layout, bytes);
        values = defaults->layout->SerializeValues(bytes);
      }
      return json{ { "Shader", asset.post->shader }, { "Values", values }, { "Textures", asset.textures } }.dump(4);
    }

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
      const json        root       = json::parse(text);
      const std::string shaderName = root.at("Shader").get<std::string>();
      // 用途の違うシェーダーへは替えない（Create で作り直す）
      if (asset.post) {
        if (!FindPostEffectDefaults(shaderName)) {
          Logger::Log("Material shader is not a post effect: %s", shaderName.c_str());
          return false;
        }
        asset.post->shader = shaderName;
        asset.post->values.Clear();
        asset.post->values.SetValues(root.value("Values", json::object()));
        asset.textures = root.value("Textures", std::map<std::string, std::string>{});
        FitTextures(asset);
        return true;
      }

      const ShaderAsset* shader = Renderer::GetInstance()->FindShader(shaderName);
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
    if (asset.post) {
      // 既定値に重ねたバイト列で編集し、変えたら全ての値を名前で持ち直す
      const PropertyBlock* defaults = FindPostEffectDefaults(asset.post->shader);
      if (!defaults) {
        return false;
      }
      std::vector<uint8_t> bytes = defaults->bytes;
      asset.post->values.Apply(*defaults->layout, bytes);
      if (!defaults->layout->DrawImGui(bytes)) {
        return false;
      }
      asset.post->values.SetValues(defaults->layout->SerializeValues(bytes));
      return true;
    }
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
    return file.empty() ? textureManager->GetEngineDefaultSRVIndex("white.png") : textureManager->GetSRVIndex(file);
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

  bool MaterialLibrary::IsPostEffectMaterial(const std::string& name) const
  {
    const auto it = assets_.find(name);
    return it != assets_.end() && it->second.post;
  }

  std::vector<std::string> MaterialLibrary::GetPostEffectMaterialNames() const
  {
    std::vector<std::string> names;
    for (const auto& [name, asset] : assets_) {
      if (asset.post) {
        names.push_back(name);
      }
    }
    return names;
  }

  std::vector<std::string> MaterialLibrary::GetPostEffectShaderNames() const
  {
    std::vector<std::string> names;
    for (const std::string& name : GetPostEffectNames()) {
      if (!FindPostEffect(name)->hidden && FindPostEffectDefaults(name)) {
        names.push_back(name);
      }
    }
    return names;
  }

  bool MaterialLibrary::LoadAsset(const std::string& name)
  {
    if (assets_.contains(name)) {
      return true;
    }
    const std::optional<std::string> text = ReadText(MaterialPath(name));
    if (!text) {
      Logger::Log("Material not found: %s", name.c_str());
      return false;
    }

    // 書かれたシェーダーで作ってから、値と描画設定を当てはめる
    std::string shaderName;
    try {
      shaderName = json::parse(*text).at("Shader").get<std::string>();
    }
    catch (const json::exception& e) {
      Logger::Log("Material load failed: %s (%s)", name.c_str(), e.what());
      return false;
    }
    if (!Create(name, shaderName) || !Apply(name, *text)) {
      assets_.erase(name);
      return false;
    }
    return true;
  }

  bool MaterialLibrary::LoadShaderFile(const std::string& path)
  {
    const std::optional<std::string> text = ReadText(path);
    return text && ApplyShaderText(path, *text);
  }

  bool MaterialLibrary::ApplyShaderText(const std::string& path, const std::string& text)
  {
    const auto fail = [&](const std::string& reason) {
      Logger::Log("Shader asset skipped: %s (%s)", path.c_str(), reason.c_str());
      loadErrors_[path] = reason;
      return false;
    };

    try {
      const json        root    = json::parse(text);
      const std::string name    = root.at("Name").get<std::string>();
      ShaderLibrary&    library = dx12_->GetShaderLibrary();
      const auto        forward = root.find("Forward");
      const auto        post    = root.find("PostEffect");
      // マテリアルの用途はシェーダーで決まるので、1 つのシェーダーは 1 つの用途にする（UE の Material Domain と同じ）
      if (forward != root.end() && post != root.end()) {
        return fail("Forward and PostEffect cannot be in one shader (make two shaders that share an .hlsli)");
      }

      // 先に全てコンパイルし、失敗したら今の登録を残す
      std::vector<GraphicsPipelineDesc> forwardPasses;
      if (forward != root.end()) {
        if (const std::string error = BuildForwardPasses(library, *forward, forwardPasses); !error.empty()) {
          return fail(error);
        }
      }

      // value() の戻り値は一時オブジェクトなので、items() で回す前に受けておく
      std::map<std::string, std::pair<float, float>> ranges;
      const json properties = root.value("Properties", json::object());
      for (const auto& [property, range] : properties.items()) {
        if (range.is_array() && range.size() == 2) {
          ranges[property] = { range[0].get<float>(), range[1].get<float>() };
        }
      }

      std::vector<PostEffectPass>             postPasses;
      std::shared_ptr<MaterialPropertyLayout> postLayout;
      std::optional<RenderEvent>              postEvent;
      std::vector<ShaderParamId>              postFade;
      if (post != root.end()) {
        if (const std::string error = BuildPostEffectPasses(library, *post, ranges, postPasses, postLayout); !error.empty()) {
          return fail(error);
        }
        if (const std::string eventName = post->value("Event", std::string()); !eventName.empty()) {
          postEvent = FindByName<RenderEvent>(kRenderEventNames, eventName);
          if (!postEvent) {
            return fail("unknown Event " + eventName);
          }
        }
        // 一時エフェクトで弱める値（"Fade": "intensity" か ["intensity", "strength"]）
        if (const auto fade = post->find("Fade"); fade != post->end()) {
          const std::vector<std::string> names = fade->is_string() ? std::vector<std::string>{ fade->get<std::string>() } : fade->get<std::vector<std::string>>();
          for (const std::string& value : names) {
            const MaterialProperty* property = postLayout->Find(MakeShaderParamId(value));
            if (!property || property->type != MaterialProperty::Type::Float) {
              return fail("Fade " + value + " is not a float value of gParam");
            }
            postFade.push_back(property->id);
          }
        }
      }

      // このファイルから前に登録したポストエフェクトのうち、名前を変えた・PostEffect を消したものを外す
      UnregisterPostEffects(path, post != root.end() ? std::string_view(name) : std::string_view());

      ShaderInfo& info = shaders_[name];
      info.path = path;
      info.ranges = std::move(ranges);
      info.defaults = root.value("Defaults", json::object());
      info.layout.reset();

      // 描画物のシェーダー
      if (forward != root.end()) {
        Renderer*    renderer = Renderer::GetInstance();
        ShaderAsset& shader   = renderer->RegisterShader(name);
        // 読み直しで Fallback を外したときに、前の影のパスを残さない
        shader = ShaderAsset(name);
        // 色を決めない影のパスは Fallback のシェーダーのものを使う
        if (const ShaderAsset* fallback = renderer->FindShader(root.value("Fallback", std::string()))) {
          for (bool instanced : { false, true }) {
            if (const GraphicsPipelineDesc* source = fallback->GetPass(ShaderPass::ShadowCaster, instanced)) {
              shader.SetPass(ShaderPass::ShadowCaster, *source, instanced);
            }
          }
        }
        shader.SetPass(ShaderPass::Forward, forwardPasses.front());
        shader.SetExtraForwardPasses({ forwardPasses.begin() + 1, forwardPasses.end() });
        shader.SetDefaultQueue(root.value("Queue", ShaderAsset::kQueueGeometry));
      }

      // ポストエフェクト（1 パスでも複数パスでも同じ経路で描く）
      if (post != root.end()) {
        info.postEffectShaders.clear();
        for (const PostEffectPass& pass : postPasses) {
          info.postEffectShaders.emplace_back(pass.ps, pass.ps->GetVersion());
        }
        PropertyBlock block{ std::move(postLayout), {} };
        block.bytes.resize(block.layout->constantSize);
        block.layout->DeserializeValues(info.defaults, block.bytes);
        RegisterPostEffect(name, {
          .defaultParam = std::move(block),
          .event        = postEvent,
          .fadeValues   = std::move(postFade),
          .execute      = [passes = std::move(postPasses)](const PostEffectFrame& frame, const EffectParam& param, RenderTexture& input, RenderTexture& output) {
            ExecutePostEffectPasses(passes, frame, param, input, output);
          },
        });
      }
      loadErrors_.erase(path);
      return true;
    }
    catch (const json::exception& e) {
      return fail(e.what());
    }
  }

  void MaterialLibrary::UnregisterPostEffects(const std::string& path, std::string_view keep)
  {
    for (auto& [name, info] : shaders_) {
      if (info.path == path && name != keep && !info.postEffectShaders.empty()) {
        UnregisterPostEffect(name);
        info.postEffectShaders.clear();
      }
    }
  }

  const std::string* MaterialLibrary::FindLoadError(const std::string& path) const
  {
    const auto it = loadErrors_.find(path);
    return it != loadErrors_.end() ? &it->second : nullptr;
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
    if (asset.post) {
      // 書いていないスロットは .shader.json の Textures のまま描くので、白を入れない
      const PropertyBlock* defaults = FindPostEffectDefaults(asset.post->shader);
      const std::vector<std::string> slots = defaults ? defaults->layout->textures : std::vector<std::string>();
      std::erase_if(asset.textures, [&](const auto& texture) { return texture.second.empty() || std::ranges::find(slots, texture.first) == slots.end(); });
      for (const std::string& slot : slots) {
        asset.post->values.Clear(slot);
        if (const auto file = asset.textures.find(slot); file != asset.textures.end()) {
          asset.post->values.SetTexture(slot, ResolveTexture(file->second));
        }
      }
      return;
    }

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
