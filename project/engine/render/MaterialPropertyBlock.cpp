#include "MaterialPropertyBlock.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "MaterialPropertyLayout.h"

namespace Tako {

  using json = nlohmann::json;

  void MaterialPropertyBlock::SetValues(std::string_view name, std::span<const float> values)
  {
    Value value{ .id = MakeShaderParamId(name), .count = static_cast<uint32_t>((std::min)(values.size(), size_t{ 16 })) };
    std::ranges::copy(values.first(value.count), value.data.begin());
    Set(value);
  }

  void MaterialPropertyBlock::SetValues(const json& values)
  {
    if (!values.is_object()) {
      return;
    }
    for (const auto& [name, value] : values.items()) {
      std::array<float, 16> data{};
      size_t                count = 0;
      if (value.is_number()) {
        data[count++] = value.get<float>();
      }
      else if (value.is_array() && value.size() <= data.size() && std::ranges::all_of(value, &json::is_number)) {
        for (const json& element : value) {
          data[count++] = element.get<float>();
        }
      }
      if (count > 0) {
        SetValues(name, std::span<const float>(data.data(), count));
      }
    }
  }

  void MaterialPropertyBlock::SetTexture(std::string_view name, uint32_t srvIndex)
  {
    SetTextureById(MakeShaderParamId(name), srvIndex);
  }

  void MaterialPropertyBlock::Clear(std::string_view name)
  {
    const ShaderParamId id     = MakeShaderParamId(name);
    const size_t        erased = std::erase_if(values_, [id](const Value& value) { return value.id == id; })
                               + std::erase_if(textures_, [id](const auto& texture) { return texture.first == id; });
    if (erased > 0) {
      ++version_;
    }
  }

  void MaterialPropertyBlock::Clear()
  {
    if (!IsEmpty()) {
      values_.clear();
      textures_.clear();
      ++version_;
    }
  }

  void MaterialPropertyBlock::Merge(const MaterialPropertyBlock& upper)
  {
    for (const Value& value : upper.values_) {
      Set(value);
    }
    for (const auto& [id, srvIndex] : upper.textures_) {
      SetTextureById(id, srvIndex);
    }
  }

  void MaterialPropertyBlock::Apply(const MaterialPropertyLayout& layout, std::span<uint8_t> constants) const
  {
    for (const Value& value : values_) {
      const MaterialProperty* property = layout.Find(value.id);
      if (!property || property->type == MaterialProperty::Type::Unsupported) {
        continue;
      }
      const uint32_t count = (std::min)(value.count, property->components);
      if (property->offset + count * sizeof(uint32_t) > constants.size()) {
        continue;
      }
      for (uint32_t c = 0; c < count; ++c) {
        uint8_t* target = constants.data() + property->offset + c * sizeof(uint32_t);
        switch (property->type) {
        case MaterialProperty::Type::Float: std::memcpy(target, &value.data[c], sizeof(float)); break;
        case MaterialProperty::Type::Int:   { const int32_t v = static_cast<int32_t>(std::lround(value.data[c])); std::memcpy(target, &v, sizeof(v)); break; }
        case MaterialProperty::Type::UInt:  { const uint32_t v = static_cast<uint32_t>(std::lround((std::max)(value.data[c], 0.0f))); std::memcpy(target, &v, sizeof(v)); break; }
        default: break;
        }
      }
    }
  }

  std::optional<Vector4> MaterialPropertyBlock::GetVector(std::string_view name) const
  {
    const auto it = std::ranges::find(values_, MakeShaderParamId(name), &Value::id);
    if (it == values_.end()) {
      return std::nullopt;
    }
    return Vector4{ it->data[0], it->data[1], it->data[2], it->data[3] };
  }

  bool MaterialPropertyBlock::Has(std::string_view name) const
  {
    const ShaderParamId id = MakeShaderParamId(name);
    return std::ranges::find(values_, id, &Value::id) != values_.end()
        || std::ranges::find(textures_, id, &std::pair<ShaderParamId, uint32_t>::first) != textures_.end();
  }

  void MaterialPropertyBlock::Set(const Value& value)
  {
    const auto it = std::ranges::find(values_, value.id, &Value::id);
    if (it == values_.end()) {
      values_.push_back(value);
    }
    else if (it->count == value.count && it->data == value.data) {
      return;
    }
    else {
      *it = value;
    }
    ++version_;
  }

  void MaterialPropertyBlock::SetTextureById(ShaderParamId id, uint32_t srvIndex)
  {
    const auto it = std::ranges::find(textures_, id, &std::pair<ShaderParamId, uint32_t>::first);
    if (it == textures_.end()) {
      textures_.emplace_back(id, srvIndex);
    }
    else if (it->second == srvIndex) {
      return;
    }
    else {
      it->second = srvIndex;
    }
    ++version_;
  }

} // namespace Tako
