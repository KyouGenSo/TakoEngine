#include "MaterialPropertyLayout.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <d3d12shader.h>
#include <format>
#include <string>

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace Tako {

  using json = nlohmann::json;

  namespace {

    // エンジンが描画のたびに渡すテクスチャ。マテリアルの値ではない
    constexpr std::string_view kEngineTextures[] = { "gShadowMap", "gDepthTexture", "gObjectMask" };

    bool ContainsIgnoreCase(std::string_view text, std::string_view word)
    {
      return std::ranges::search(text, word, [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }).begin() != text.end();
    }

    void AddMembers(ID3D12ShaderReflectionType* type, uint32_t offset, const std::string& name,
                    const std::map<std::string, std::pair<float, float>>& ranges, std::vector<MaterialProperty>& out)
    {
      D3D12_SHADER_TYPE_DESC desc{};
      type->GetDesc(&desc);
      if (desc.Class == D3D_SVC_STRUCT && desc.Elements == 0) {
        for (UINT i = 0; i < desc.Members; ++i) {
          ID3D12ShaderReflectionType* member = type->GetMemberTypeByIndex(i);
          D3D12_SHADER_TYPE_DESC memberDesc{};
          member->GetDesc(&memberDesc);
          const std::string memberName = type->GetMemberTypeName(i);
          AddMembers(member, offset + memberDesc.Offset, name.empty() ? memberName : name + "." + memberName, ranges, out);
        }
        return;
      }
      // 16 バイト境界合わせの詰め物は編集しない
      if (ContainsIgnoreCase(name, "padding")) {
        return;
      }

      MaterialProperty property{ .name = name, .type = MaterialProperty::Type::Unsupported, .components = desc.Columns, .offset = offset };
      if (desc.Elements == 0 && (desc.Class == D3D_SVC_SCALAR || desc.Class == D3D_SVC_VECTOR)) {
        switch (desc.Type) {
        case D3D_SVT_FLOAT: property.type = MaterialProperty::Type::Float; break;
        case D3D_SVT_INT:
        case D3D_SVT_BOOL:  property.type = MaterialProperty::Type::Int; break;
        case D3D_SVT_UINT:  property.type = MaterialProperty::Type::UInt; break;
        default: break;
        }
      }
      property.isColor = property.type == MaterialProperty::Type::Float && property.components >= 3 && ContainsIgnoreCase(name, "color");
      if (const auto range = ranges.find(name); range != ranges.end()) {
        property.min = range->second.first;
        property.max = range->second.second;
      }
      out.push_back(std::move(property));
    }

    bool Fits(const MaterialProperty& property, size_t size)
    {
      return property.type != MaterialProperty::Type::Unsupported && property.offset + property.components * sizeof(uint32_t) <= size;
    }

  } // anonymous namespace

  std::shared_ptr<MaterialPropertyLayout> MaterialPropertyLayout::Reflect(ID3D12ShaderReflection* ps, std::string_view cbufferName, const std::map<std::string, std::pair<float, float>>& ranges)
  {
    auto layout = std::make_shared<MaterialPropertyLayout>();
    if (!ps) {
      return layout;
    }

    D3D12_SHADER_DESC shaderDesc{};
    ps->GetDesc(&shaderDesc);
    for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
      D3D12_SHADER_INPUT_BIND_DESC bind{};
      ps->GetResourceBindingDesc(i, &bind);
      const std::string_view name = bind.Name;

      if (bind.Type == D3D_SIT_CBUFFER && name == cbufferName) {
        ID3D12ShaderReflectionConstantBuffer* buffer = ps->GetConstantBufferByName(bind.Name);
        D3D12_SHADER_BUFFER_DESC bufferDesc{};
        buffer->GetDesc(&bufferDesc);
        layout->constantSize = bufferDesc.Size;
        for (UINT v = 0; v < bufferDesc.Variables; ++v) {
          ID3D12ShaderReflectionVariable* variable = buffer->GetVariableByIndex(v);
          D3D12_SHADER_VARIABLE_DESC variableDesc{};
          variable->GetDesc(&variableDesc);
          // ConstantBuffer<構造体> は定数バッファと同名の変数 1 つになるので、構造体のメンバーを直接並べる
          const std::string prefix = (bufferDesc.Variables == 1 && name == variableDesc.Name) ? "" : variableDesc.Name;
          AddMembers(variable->GetType(), variableDesc.StartOffset, prefix, ranges, layout->properties);
        }
      }
      else if (bind.Type == D3D_SIT_TEXTURE && bind.Dimension == D3D_SRV_DIMENSION_TEXTURE2D && std::ranges::find(kEngineTextures, name) == std::end(kEngineTextures)) {
        layout->textures.emplace_back(name);
      }
    }
    return layout;
  }

  json MaterialPropertyLayout::SerializeValues(std::span<const uint8_t> constants) const
  {
    json values = json::object();
    for (const MaterialProperty& property : properties) {
      if (!Fits(property, constants.size())) {
        continue;
      }
      json value = json::array();
      for (uint32_t c = 0; c < property.components; ++c) {
        const uint8_t* source = constants.data() + property.offset + c * sizeof(uint32_t);
        switch (property.type) {
        case MaterialProperty::Type::Float: {
          // float を double に広げた桁（0.2 → 0.20000000298...）を書かないよう、float の精度で丸めてから渡す
          float v;
          std::memcpy(&v, source, sizeof(v));
          value.push_back(std::stod(std::format("{:.7g}", v)));
          break;
        }
        case MaterialProperty::Type::Int:   { int32_t v; std::memcpy(&v, source, sizeof(v)); value.push_back(v); break; }
        case MaterialProperty::Type::UInt:  { uint32_t v; std::memcpy(&v, source, sizeof(v)); value.push_back(v); break; }
        default: break;
        }
      }
      values[property.name] = property.components == 1 ? value[0] : value;
    }
    return values;
  }

  void MaterialPropertyLayout::DeserializeValues(const json& values, std::span<uint8_t> constants) const
  {
    for (const MaterialProperty& property : properties) {
      const auto it = values.find(property.name);
      if (it == values.end() || !Fits(property, constants.size())) {
        continue;
      }
      const json elements = it->is_array() ? *it : json::array({ *it });
      if (elements.size() != property.components || !std::ranges::all_of(elements, &json::is_number)) {
        continue;
      }
      for (uint32_t c = 0; c < property.components; ++c) {
        uint8_t* target = constants.data() + property.offset + c * sizeof(uint32_t);
        switch (property.type) {
        case MaterialProperty::Type::Float: { const float v = elements[c].get<float>(); std::memcpy(target, &v, sizeof(v)); break; }
        case MaterialProperty::Type::Int:   { const int32_t v = elements[c].get<int32_t>(); std::memcpy(target, &v, sizeof(v)); break; }
        case MaterialProperty::Type::UInt:  { const uint32_t v = elements[c].get<uint32_t>(); std::memcpy(target, &v, sizeof(v)); break; }
        default: break;
        }
      }
    }
  }

  bool MaterialPropertyLayout::DrawImGui(std::span<uint8_t> constants) const
  {
#ifdef _DEBUG
    bool changed = false;
    for (const MaterialProperty& property : properties) {
      if (!Fits(property, constants.size())) {
        ImGui::TextDisabled("%s (unsupported type)", property.name.c_str());
        continue;
      }
      void*      data     = constants.data() + property.offset;
      const int  count    = static_cast<int>(property.components);
      const bool hasRange = property.min < property.max;
      switch (property.type) {
      case MaterialProperty::Type::Float:
        if (property.isColor) {
          changed |= count == 3 ? ImGui::ColorEdit3(property.name.c_str(), static_cast<float*>(data)) : ImGui::ColorEdit4(property.name.c_str(), static_cast<float*>(data));
        }
        else {
          const float speed = hasRange ? (property.max - property.min) / 200.0f : 0.01f;
          changed |= ImGui::DragScalarN(property.name.c_str(), ImGuiDataType_Float, data, count, speed, hasRange ? &property.min : nullptr, hasRange ? &property.max : nullptr);
        }
        break;
      case MaterialProperty::Type::Int: {
        const int32_t min = static_cast<int32_t>(property.min);
        const int32_t max = static_cast<int32_t>(property.max);
        changed |= ImGui::DragScalarN(property.name.c_str(), ImGuiDataType_S32, data, count, 0.1f, hasRange ? &min : nullptr, hasRange ? &max : nullptr);
        break;
      }
      case MaterialProperty::Type::UInt: {
        const uint32_t min = static_cast<uint32_t>((std::max)(property.min, 0.0f));
        const uint32_t max = static_cast<uint32_t>((std::max)(property.max, 0.0f));
        changed |= ImGui::DragScalarN(property.name.c_str(), ImGuiDataType_U32, data, count, 0.1f, hasRange ? &min : nullptr, hasRange ? &max : nullptr);
        break;
      }
      default:
        break;
      }
    }
    return changed;
#else
    (void)constants;
    return false;
#endif
  }

} // namespace Tako
