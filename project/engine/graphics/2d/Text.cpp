#include "Text.h"
#include "DX12Basic.h"
#include "Font.h"
#include "Mat4x4Func.h"
#include "Renderer.h"
#include "StringUtility.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <string_view>
#include <vector>

namespace Tako {

  namespace {

    constexpr uint32_t kVerticesPerQuad = 4;
    constexpr uint32_t kIndicesPerQuad  = 6;
    constexpr uint32_t kMinCapacity     = 16;
    constexpr float    kMaxOutline      = 0.45f;  ///< SDF の値。0.5 で距離場の端に届くので、縁をなめらかにする分を残す

    // 行頭に置けない字（閉じ括弧・句読点・小書きのかな・長音など）と、行末に置けない字（開き括弧）
    constexpr std::u32string_view kNoLineStart =
      U",.!?:;)]}、。，．・：；？！゛゜ヽヾゝゞ々ー）］｝〕〉》」』】〙〗〟’”…‥ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ";
    constexpr std::u32string_view kNoLineEnd = U"([{（［｛〔〈《「『【〘〖〝‘“";

    bool IsSpace(char32_t code) {
      return code == U' ' || code == U'\t' || code == U'　';
    }

    /// <summary>
    /// 単語に区切らず、どの字の間でも折れる字（かな・漢字・全角の記号）
    /// </summary>
    bool IsWide(char32_t code) {
      return (code >= 0x3000 && code <= 0x30FF) || (code >= 0x3400 && code <= 0x4DBF) || (code >= 0x4E00 && code <= 0x9FFF) ||
        (code >= 0xF900 && code <= 0xFAFF) || (code >= 0xFF00 && code <= 0xFFEF) || (code >= 0x20000 && code <= 0x2FFFF);
    }

    /// <summary>
    /// previous と current の間で行を折ってよいか
    /// </summary>
    bool CanBreakBetween(char32_t previous, char32_t current) {
      // ponytail: 禁則は上の 2 つの表だけ。英単語の途中のハイフンや、数字と単位の組は見ない。足りなくなったら UAX #14 にする
      if (kNoLineStart.find(current) != std::u32string_view::npos || kNoLineEnd.find(previous) != std::u32string_view::npos) {
        return false;
      }
      if (IsSpace(current)) {
        return false;
      }
      return IsSpace(previous) || IsWide(previous) || IsWide(current);
    }

    std::u32string Decode(const std::string& utf8) {
      const std::wstring utf16 = StringUtility::ConvertString(utf8);
      std::u32string     codes;
      codes.reserve(utf16.size());
      for (size_t i = 0; i < utf16.size(); ++i) {
        char32_t code = utf16[i];
        if (code >= 0xD800 && code <= 0xDBFF && i + 1 < utf16.size()) {
          code = 0x10000 + ((code - 0xD800) << 10) + (utf16[++i] - 0xDC00);
        }
        codes.push_back(code);
      }
      return codes;
    }

  } // namespace

  Text::Text() {
    Renderer*  renderer = Renderer::GetInstance();
    DX12Basic* dx12     = renderer->GetDX12Basic();

    material_ = std::make_unique<Material>(renderer->FindShader("Text"));
    MaterialConstants& constants = material_->Constants<MaterialConstants>();
    constants.color        = color_;
    constants.outlineColor = outlineColor_;
    constants.unitRange    = { Font::GetUnitRange(), Font::GetUnitRange() };
    constants.outline      = 0.0f;
    constants.padding      = 0.0f;

    transformationMatrixResource_ = dx12->MakeBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP   = Mat4x4::MakeIdentity();
    transformationMatrixData_->world = Mat4x4::MakeIdentity();
  }

  Text::~Text() = default;

  void Text::SetLayout(const Layout& layout) {
    if (layout != layout_) {
      layout_  = layout;
      isDirty_ = true;
    }
  }

  void Text::Update() {
    if (isDirty_) {
      Rebuild();
    }

    // 縁取りの太さを SDF の値にする：size 単位 → em → アトラス px → SDF の値
    const float outlineInAtlasPx = layout_.size > 0.0f ? outlineWidth_ / layout_.size * Font::kBakeSize : 0.0f;
    MaterialConstants& constants = material_->Constants<MaterialConstants>();
    constants.color        = color_;
    constants.outlineColor = outlineColor_;
    constants.outline      = std::clamp(outlineInAtlasPx / (Font::GetUnitRange() * static_cast<float>(Font::kAtlasSize)), 0.0f, kMaxOutline);

    const Renderer* renderer = Renderer::GetInstance();
    const Matrix4x4 world    = Mat4x4::MakeAffine({ scale_.x, scale_.y, 1.0f }, Vector3{ 0.0f, 0.0f, rotation_ }, { pos_.x, pos_.y, 0.0f });
    transformationMatrixData_->WVP   = Mat4x4::Multiply(world, Mat4x4::Multiply(renderer->GetScreenViewMatrix(), renderer->GetScreenProjectionMatrix()));
    transformationMatrixData_->world = world;
  }

  void Text::Draw() {
    if (quadCount_ == 0 || !font_) {
      return;
    }
    font_->Upload();

    DrawItem item;
    item.material        = material_.get();
    item.vertexBuffer    = &vertexBufferView_;
    item.indexBuffer     = &indexBufferView_;
    item.indexCount      = quadCount_ * kIndicesPerQuad;
    item.transform       = transformationMatrixResource_->GetGPUVirtualAddress();
    item.textureOverride = font_->GetSrvIndex();
    item.sortingOrder    = sortingOrder_;
    item.depthSort       = false;
    item.castShadow      = false;
    Renderer::GetInstance()->Submit(item);
  }

  void Text::Rebuild() {
    isDirty_   = false;
    quadCount_ = 0;
    font_      = Font::Get(layout_.font);
    if (!font_) {
      return;
    }

    struct Placed {
      const Font::Glyph* glyph;
      char32_t           code;
      float              x;  ///< 行の左端からペン位置まで
    };
    struct Line {
      size_t begin;
      size_t end;
      float  width;  ///< 末尾の空白を除いた幅
    };
    constexpr size_t kNoBreak = static_cast<size_t>(-1);

    const float size = layout_.size;
    std::vector<Placed> placed;
    std::vector<Line>   lines;
    size_t              lineBegin    = 0;
    float               penX         = 0.0f;
    float               lineWidth    = 0.0f;
    size_t              breakIndex   = kNoBreak;  ///< 今の行で最後に見つけた、折れる位置（この字の前で折る）
    float               breakWidth   = 0.0f;      ///< そこで折ったときの行の幅
    const Font::Glyph*  previous     = nullptr;
    char32_t            previousCode = 0;

    auto endLine = [&](size_t end, float width) {
      lines.push_back({ lineBegin, end, width });
      lineBegin = end;
    };

    for (const char32_t code : Decode(layout_.string)) {
      if (code == U'\r') {
        continue;
      }
      if (code == U'\n') {
        endLine(placed.size(), lineWidth);
        penX       = 0.0f;
        lineWidth  = 0.0f;
        breakIndex = kNoBreak;
        previous   = nullptr;
        continue;
      }

      const Font::Glyph* glyph = font_->FindGlyph(code);
      if (!glyph) {
        isDirty_ = true;
        continue;
      }

      if (previous) {
        penX += font_->GetKerning(*previous, *glyph) * size;
        if (layout_.wrap && placed.size() > lineBegin && CanBreakBetween(previousCode, code)) {
          breakIndex = placed.size();
          breakWidth = lineWidth;
        }
      }
      placed.push_back({ glyph, code, penX });
      penX += glyph->advance * size;
      if (!IsSpace(code)) {
        lineWidth = penX;
      }

      // 箱を越えたら、最後に見つけた位置から後ろを次の行へ移す
      if (layout_.wrap && lineWidth > layout_.boxSize.x && breakIndex != kNoBreak) {
        const float shift = placed[breakIndex].x;
        for (size_t i = breakIndex; i < placed.size(); ++i) {
          placed[i].x -= shift;
        }
        endLine(breakIndex, breakWidth);
        penX      -= shift;
        lineWidth -= shift;
        breakIndex = kNoBreak;
      }

      previous     = glyph;
      previousCode = code;
    }
    endLine(placed.size(), lineWidth);

#ifdef _DEBUG
    // 折り返しの検算: 箱を越えた行は、中に折れる位置が無い（分けられない 1 語）こと
    for (const Line& line : lines) {
      if (!layout_.wrap || line.width <= layout_.boxSize.x) {
        continue;
      }
      for (size_t i = line.begin + 1; i < line.end; ++i) {
        assert(!CanBreakBetween(placed[i - 1].code, placed[i].code) && "Text: a line wider than the box could have been wrapped");
      }
    }
#endif

    const auto visibleCount = static_cast<uint32_t>(std::ranges::count_if(placed, [](const Placed& p) { return p.glyph->size.x > 0.0f; }));
    if (visibleCount == 0) {
      return;
    }
    Reserve(visibleCount);

    const float   ascent      = font_->GetAscent() * size;
    const float   lineHeight  = (font_->GetAscent() - font_->GetDescent()) * size;
    const float   lineAdvance = (lineHeight + font_->GetLineGap() * size) * layout_.lineSpacing;
    const float   blockHeight = lineHeight + lineAdvance * static_cast<float>(lines.size() - 1);
    const Vector2 boxMin      = { -layout_.pivot.x * layout_.boxSize.x, -layout_.pivot.y * layout_.boxSize.y };

    float       baseline = boxMin.y + (layout_.boxSize.y - blockHeight) * layout_.align.y + ascent;
    VertexData* vertex   = vertexData_;
    for (const Line& line : lines) {
      const float lineLeft = boxMin.x + (layout_.boxSize.x - line.width) * layout_.align.x;
      for (size_t i = line.begin; i < line.end; ++i) {
        const Font::Glyph& glyph = *placed[i].glyph;
        if (glyph.size.x <= 0.0f) {
          continue;
        }
        const float left   = lineLeft + placed[i].x + glyph.offset.x * size;
        const float top    = baseline + glyph.offset.y * size;
        const float right  = left + glyph.size.x * size;
        const float bottom = top + glyph.size.y * size;

        // 頂点の並びは左上・右上・左下・右下（Sprite と同じ）
        vertex[0] = { { left, top, 0.0f, 1.0f }, { glyph.uvMin.x, glyph.uvMin.y } };
        vertex[1] = { { right, top, 0.0f, 1.0f }, { glyph.uvMax.x, glyph.uvMin.y } };
        vertex[2] = { { left, bottom, 0.0f, 1.0f }, { glyph.uvMin.x, glyph.uvMax.y } };
        vertex[3] = { { right, bottom, 0.0f, 1.0f }, { glyph.uvMax.x, glyph.uvMax.y } };
        vertex += kVerticesPerQuad;
      }
      baseline += lineAdvance;
    }
    quadCount_ = visibleCount;
  }

  void Text::Reserve(uint32_t quadCount) {
    if (quadCount <= capacity_) {
      return;
    }
    capacity_ = (std::max)(kMinCapacity, std::bit_ceil(quadCount));

    DX12Basic* dx12 = Renderer::GetInstance()->GetDX12Basic();

    const UINT vertexBytes = static_cast<UINT>(sizeof(VertexData) * kVerticesPerQuad * capacity_);
    vertexResource_ = dx12->MakeBufferResource(vertexBytes);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes    = vertexBytes;
    vertexBufferView_.StrideInBytes  = sizeof(VertexData);

    const UINT indexBytes = static_cast<UINT>(sizeof(uint32_t) * kIndicesPerQuad * capacity_);
    indexResource_ = dx12->MakeBufferResource(indexBytes);
    uint32_t* indices = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indices));
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes    = indexBytes;
    indexBufferView_.Format         = DXGI_FORMAT_R32_UINT;

    // 三角形の並びは左下・左上・右下 / 右上・右下・左上（Sprite と同じ）
    for (uint32_t quad = 0; quad < capacity_; ++quad) {
      const uint32_t base = quad * kVerticesPerQuad;
      uint32_t*      out  = indices + quad * kIndicesPerQuad;
      out[0] = base + 2; out[1] = base + 0; out[2] = base + 3;
      out[3] = base + 1; out[4] = base + 3; out[5] = base + 0;
    }
  }

} // namespace Tako
