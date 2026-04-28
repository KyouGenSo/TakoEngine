#pragma once
#include <string>
#include <string_view>

namespace Tako::EnginePaths {

  inline constexpr std::string_view kEngineRoot = "EngineResources/";
  inline constexpr std::string_view kEngineShaders = "EngineResources/shaders/";
  inline constexpr std::string_view kEngineTextures = "EngineResources/Texture/";
  inline constexpr std::string_view kEngineModels = "EngineResources/Model/";

  inline constexpr std::wstring_view kEngineShadersW = L"EngineResources/shaders/";

  inline std::wstring ShaderPath(std::wstring_view fileName) {
    return std::wstring(kEngineShadersW) + std::wstring(fileName);
  }

  inline std::string TexturePath(std::string_view fileName) {
    return std::string(kEngineTextures) + std::string(fileName);
  }

  inline std::string ModelPath(std::string_view fileName) {
    return std::string(kEngineModels) + std::string(fileName);
  }

} // namespace Tako::EnginePaths
