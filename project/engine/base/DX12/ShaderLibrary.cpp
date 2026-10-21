#include "ShaderLibrary.h"

#include <cassert>
#include <format>
#include <system_error>

#include "EnginePaths.h"
#include "StringUtility.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

#pragma comment(lib, "dxcompiler.lib")

namespace Tako {

  namespace {

    const wchar_t* Profile(ShaderStage stage)
    {
      switch (stage) {
      case ShaderStage::Vertex:  return L"vs_6_0";
      case ShaderStage::Pixel:   return L"ps_6_0";
      case ShaderStage::Compute: return L"cs_6_0";
      }
      return L"";
    }

    std::wstring MakeCacheKey(const ShaderKey& key)
    {
      std::wstring text = std::format(L"{}|{}|{}", key.file, key.entry, static_cast<int>(key.stage));
      for (const std::wstring& define : key.defines) {
        text += L"|" + define;
      }
      return text;
    }

    enum class LogLevel { Info, Warning, Error };

    void Log([[maybe_unused]] const std::string& message, [[maybe_unused]] LogLevel level)
    {
#ifdef _DEBUG
      const DebugUIManager::LogType type = (level == LogLevel::Error)   ? DebugUIManager::LogType::Error
                                         : (level == LogLevel::Warning) ? DebugUIManager::LogType::Warning
                                                                        : DebugUIManager::LogType::Info;
      DebugUIManager::GetInstance()->AddLog(message, type);
#endif
    }

    std::filesystem::file_time_type LastWriteTime(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto time = std::filesystem::last_write_time(path, error);
      return error ? std::filesystem::file_time_type{} : time;
    }

  } // namespace

  HRESULT STDMETHODCALLTYPE ShaderLibrary::RecordingIncludeHandler::LoadSource(LPCWSTR fileName, IDxcBlob** includeSource)
  {
    const HRESULT hr = inner->LoadSource(fileName, includeSource);
    if (SUCCEEDED(hr) && loaded) {
      loaded->emplace_back(fileName);
    }
    return hr;
  }

  HRESULT STDMETHODCALLTYPE ShaderLibrary::RecordingIncludeHandler::QueryInterface(REFIID riid, void** object)
  {
    if (riid == __uuidof(IDxcIncludeHandler) || riid == __uuidof(IUnknown)) {
      *object = static_cast<IDxcIncludeHandler*>(this);
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  void ShaderLibrary::Initialize()
  {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
    assert(SUCCEEDED(hr));
    hr = utils_->CreateDefaultIncludeHandler(&includeHandler_.inner);
    assert(SUCCEEDED(hr));

    engineShaderDir_ = std::filesystem::path(EnginePaths::kEngineShadersW);
#ifdef _DEBUG
    // 作業ディレクトリのシェーダーはビルド時のコピーなので、編集が即反映されるようソースを直接読む
    const std::filesystem::path sourceDir = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / L"resources" / L"shaders";
    if (std::filesystem::is_directory(sourceDir)) {
      engineShaderDir_ = sourceDir;
    }
#endif
  }

  const Shader* ShaderLibrary::Get(const ShaderKey& key)
  {
    const Shader* shader = TryGet(key);
    assert(shader && "ShaderLibrary: compile failed");
    return shader;
  }

  const Shader* ShaderLibrary::TryGet(const ShaderKey& key)
  {
    const std::wstring cacheKey = MakeCacheKey(key);
    if (const auto it = shaders_.find(cacheKey); it != shaders_.end()) {
      return it->second.get();
    }
    // 失敗したキーは登録しないので、ファイルを直せば次の呼び出しでコンパイルし直す
    auto shader = std::make_unique<Shader>();
    shader->key_ = key;
    if (!Compile(*shader)) {
      return nullptr;
    }
    return (shaders_[cacheKey] = std::move(shader)).get();
  }

  bool ShaderLibrary::ReloadChangedShaders()
  {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReloadCheck_ < kReloadCheckInterval) {
      return false;
    }
    lastReloadCheck_ = now;

    bool reloaded = false;
    for (auto& [cacheKey, shader] : shaders_) {
      bool changed = false;
      for (size_t i = 0; i < shader->sources_.size() && !changed; ++i) {
        changed = LastWriteTime(shader->sources_[i]) != shader->sourceTimes_[i];
      }
      if (!changed) {
        continue;
      }
      if (Compile(*shader)) {
        reloaded = true;
        Log(StringUtility::ConvertString(std::format(L"Shader reloaded: {}", shader->key_.file)), LogLevel::Info);
      }
      else {
        // 次の保存まで再試行しないよう、失敗した時点の時刻を覚える
        for (size_t i = 0; i < shader->sources_.size(); ++i) {
          shader->sourceTimes_[i] = LastWriteTime(shader->sources_[i]);
        }
      }
    }
    return reloaded;
  }

  bool ShaderLibrary::Compile(Shader& shader)
  {
    const ShaderKey& key = shader.key_;
    const std::filesystem::path path = ResolvePath(key.file);
    // エディタに出せるよう、失敗の文をファイルごとに残す
    const auto fail = [&](const std::string& message) {
      Log(message, LogLevel::Error);
      compileErrors_[key.file] = message;
      return false;
    };

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
    if (FAILED(utils_->LoadFile(path.c_str(), nullptr, &source))) {
      return fail(std::format("Shader not found: {}", path.string()));
    }
    const DxcBuffer sourceBuffer{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8 };

    const std::wstring pathText = path.wstring();
    const std::wstring includeDir = path.parent_path().wstring();
    // ゲーム側のシェーダーからもエンジンの .hlsli を include できるようにする
    const std::wstring engineIncludeDir = engineShaderDir_.wstring();
    std::vector<LPCWSTR> arguments = {
      pathText.c_str(),
      L"-E", key.entry.c_str(),
      L"-T", Profile(key.stage),
      L"-I", includeDir.c_str(),
      L"-I", engineIncludeDir.c_str(),
      L"-Zpr",  // 行列を行優先で詰める（C++ 側の Matrix4x4 と合わせる）
#ifdef _DEBUG
      L"-Zi", L"-Qembed_debug", L"-Od",
#else
      L"-O3",
#endif
    };
    for (const std::wstring& define : key.defines) {
      arguments.push_back(L"-D");
      arguments.push_back(define.c_str());
    }

    std::vector<std::filesystem::path> includes;
    includeHandler_.loaded = &includes;
    Microsoft::WRL::ComPtr<IDxcResult> result;
    HRESULT hr = compiler_->Compile(&sourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()),
      &includeHandler_, IID_PPV_ARGS(&result));
    includeHandler_.loaded = nullptr;

    HRESULT status = E_FAIL;
    if (SUCCEEDED(hr)) {
      result->GetStatus(&status);
    }
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> messages;
    if (result) {
      result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&messages), nullptr);
    }
    const bool hasMessages = messages && messages->GetStringLength() != 0;
    if (FAILED(hr) || FAILED(status)) {
      return fail(std::format("Shader compile error: {}\n{}", path.string(), hasMessages ? messages->GetStringPointer() : ""));
    }
    if (hasMessages) {
      Log(std::format("Shader compile warning: {}\n{}", path.string(), messages->GetStringPointer()), LogLevel::Warning);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> blob;
    Microsoft::WRL::ComPtr<IDxcBlob> reflectionBlob;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
    result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);
    if (!blob || !reflectionBlob) {
      return fail(std::format("Shader output missing: {}", path.string()));
    }
    const DxcBuffer reflectionBuffer{ reflectionBlob->GetBufferPointer(), reflectionBlob->GetBufferSize(), 0 };
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
    if (FAILED(utils_->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection)))) {
      return fail(std::format("Shader reflection failed: {}", path.string()));
    }

    compileErrors_.erase(key.file);
    shader.blob_ = blob;
    shader.reflection_ = reflection;
    ++shader.version_;

    shader.sources_.clear();
    shader.sources_.push_back(path);
    shader.sources_.insert(shader.sources_.end(), includes.begin(), includes.end());
    shader.sourceTimes_.clear();
    for (const std::filesystem::path& file : shader.sources_) {
      shader.sourceTimes_.push_back(LastWriteTime(file));
    }
    return true;
  }

  std::filesystem::path ShaderLibrary::ResolvePath(const std::wstring& file) const
  {
    const std::filesystem::path path(file);
    return path.has_parent_path() ? path : engineShaderDir_ / path;
  }

  const std::string* ShaderLibrary::FindCompileError(const std::wstring& file) const
  {
    const auto it = compileErrors_.find(file);
    return it != compileErrors_.end() ? &it->second : nullptr;
  }

} // namespace Tako
