#include "Font.h"
#include "DX12Basic.h"
#include "Logger.h"
#include "Renderer.h"
#include "SrvManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

// imgui は Debug でしかリンクしないので、同じヘッダからこのファイル専用の実装を作る。
// 使わない static 関数の警告（C4505）は翻訳単位の終わりに出るので、pop の外で止める
#pragma warning(disable : 4505)
#pragma warning(push, 0)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "externals/imgui/imstb_truetype.h"
#pragma warning(pop)

namespace Tako {

  namespace {

    constexpr unsigned char kOnEdgeValue    = 128;                                               ///< 輪郭にあたる SDF の値
    constexpr float         kPixelDistScale = static_cast<float>(kOnEdgeValue) / Font::kSpread;  ///< アトラス 1px 離れるごとに SDF の値が変わる量

    /// <summary>
    /// キー → フォント。読めなかったキーは nullptr で覚え、毎フレーム読み直さない
    /// </summary>
    std::unordered_map<std::string, std::unique_ptr<Font>>& Cache() {
      static std::unordered_map<std::string, std::unique_ptr<Font>> cache;
      return cache;
    }

    int remainingNewGlyphs = 0;  ///< このフレームにあと何字の距離場を作れるか（Font::BeginFrame が戻す）

    std::string FilePath(const std::string& key) {
      return key.starts_with("EngineResources/") ? key : Font::kDirectory + key;
    }

    /// <summary>
    /// 字 1 つの距離場。左上が (offsetX, offsetY)（ベースライン上のペン位置から、下が正）
    /// </summary>
    struct GlyphSdf {
      std::vector<unsigned char> pixels;  ///< 空白など形の無い字は空
      int                        width   = 0;
      int                        height  = 0;
      int                        offsetX = 0;
      int                        offsetY = 0;
    };

    /// <summary>
    /// 字の輪郭を線分に直し、ピクセルごとに一番近い線分までの距離と内外を求める。
    /// stbtt_GetGlyphSDF は 3 次曲線を無視するので、CFF（.otf）のフォントでは使えない
    /// </summary>
    /// <param name="scale">フォントの単位 → アトラスの px</param>
    GlyphSdf MakeGlyphSdf(const stbtt_fontinfo& info, int glyph, float scale) {
      constexpr float kFlatness = 0.2f;  ///< 曲線を線分に直すときに許すずれ（アトラスの px）
      const int       spread    = Font::kSpread;

      GlyphSdf sdf;
      int      left   = 0;
      int      top    = 0;
      int      right  = 0;
      int      bottom = 0;
      stbtt_GetGlyphBitmapBox(&info, glyph, scale, scale, &left, &top, &right, &bottom);
      if (left == right || top == bottom) {
        return sdf;
      }

      stbtt_vertex* vertices       = nullptr;
      const int     vertexCount    = stbtt_GetGlyphShape(&info, glyph, &vertices);
      int*          contourLengths = nullptr;
      int           contourCount   = 0;
      stbtt__point* points         = stbtt_FlattenCurves(vertices, vertexCount, kFlatness / scale, &contourLengths, &contourCount, nullptr);
      stbtt_FreeShape(&info, vertices);
      if (!points) {
        return sdf;
      }

      sdf.offsetX = left - spread;
      sdf.offsetY = top - spread;
      sdf.width   = right - left + spread * 2;
      sdf.height  = bottom - top + spread * 2;

      // 輪郭は閉じた多角形の並び。フォントは上が正なので、左上が原点・下が正のビットマップの座標へ直す
      struct Segment {
        float ax, ay, bx, by;
      };
      std::vector<Segment> segments;
      for (int contour = 0, first = 0; contour < contourCount; first += contourLengths[contour++]) {
        const int count = contourLengths[contour];
        for (int i = 0, j = count - 1; i < count; j = i++) {
          const stbtt__point& a = points[first + j];
          const stbtt__point& b = points[first + i];
          segments.push_back({ a.x * scale - static_cast<float>(sdf.offsetX), -a.y * scale - static_cast<float>(sdf.offsetY),
                               b.x * scale - static_cast<float>(sdf.offsetX), -b.y * scale - static_cast<float>(sdf.offsetY) });
        }
      }
      STBTT_free(points, nullptr);
      STBTT_free(contourLengths, nullptr);

      // 距離（2 乗）。線分ごとに、spread だけ広げた範囲のピクセルだけを更新する（それより遠い所は値が頭打ちになる）
      const float        farthest = static_cast<float>((spread + 1) * (spread + 1));
      std::vector<float> distanceSq(static_cast<size_t>(sdf.width) * sdf.height, farthest);
      float* const       distances = distanceSq.data();
      for (const Segment& s : segments) {
        const int   minX     = (std::max)(0, static_cast<int>(std::floor((std::min)(s.ax, s.bx))) - spread);
        const int   maxX     = (std::min)(sdf.width - 1, static_cast<int>(std::ceil((std::max)(s.ax, s.bx))) + spread);
        const int   minY     = (std::max)(0, static_cast<int>(std::floor((std::min)(s.ay, s.by))) - spread);
        const int   maxY     = (std::min)(sdf.height - 1, static_cast<int>(std::ceil((std::max)(s.ay, s.by))) + spread);
        const float dx       = s.bx - s.ax;
        const float dy       = s.by - s.ay;
        const float lengthSq = dx * dx + dy * dy;
        for (int y = minY; y <= maxY; ++y) {
          float* const row = distances + static_cast<size_t>(y) * sdf.width;
          for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f - s.ax;
            const float py = static_cast<float>(y) + 0.5f - s.ay;
            const float t  = lengthSq > 0.0f ? std::clamp((px * dx + py * dy) / lengthSq, 0.0f, 1.0f) : 0.0f;
            const float ex = px - dx * t;
            const float ey = py - dy * t;
            row[x] = (std::min)(row[x], ex * ex + ey * ey);
          }
        }
      }

      // 内外は行ごとに、ピクセルの中心の高さを横切る線分を左から数えて決める（巻き数が 0 でなければ内側）
      sdf.pixels.resize(distanceSq.size());
      std::vector<std::pair<float, int>> crossings;
      for (int y = 0; y < sdf.height; ++y) {
        const float centerY = static_cast<float>(y) + 0.5f;
        crossings.clear();
        for (const Segment& s : segments) {
          if ((s.ay <= centerY) != (s.by <= centerY)) {
            crossings.emplace_back(s.ax + (centerY - s.ay) * (s.bx - s.ax) / (s.by - s.ay), s.by > s.ay ? 1 : -1);
          }
        }
        std::ranges::sort(crossings);

        int    winding = 0;
        size_t next    = 0;
        for (int x = 0; x < sdf.width; ++x) {
          const float centerX = static_cast<float>(x) + 0.5f;
          while (next < crossings.size() && crossings[next].first <= centerX) {
            winding += crossings[next++].second;
          }
          const size_t index    = static_cast<size_t>(y) * sdf.width + x;
          const float  distance = std::sqrt(distances[index]);
          const float  value    = static_cast<float>(kOnEdgeValue) + (winding != 0 ? distance : -distance) * kPixelDistScale;
          sdf.pixels[index] = static_cast<unsigned char>(std::clamp(value, 0.0f, 255.0f));
        }
      }
      return sdf;
    }

  } // namespace

  Font::Font() = default;

  Font::~Font() {
    SrvManager::GetInstance()->Free(srvIndex_);
  }

  Font* Font::Get(const std::string& key) {
    const std::string resolved = key.empty() ? std::string(kDefaultKey) : key;

    auto& cache = Cache();
    auto  it    = cache.find(resolved);
    if (it == cache.end()) {
      std::unique_ptr<Font> font(new Font());
      if (!font->Load(resolved)) {
        Logger::Log("Font not found or unreadable: " + resolved + "\n");
        font.reset();
      }
      it = cache.emplace(resolved, std::move(font)).first;
    }
    if (it->second) {
      return it->second.get();
    }
    return resolved == kDefaultKey ? nullptr : Get(kDefaultKey);
  }

  void Font::BeginFrame() {
    remainingNewGlyphs = kMaxNewGlyphsPerFrame;
  }

  void Font::Finalize() {
    Cache().clear();
  }

  float Font::GetUnitRange() {
    return 255.0f / kPixelDistScale / static_cast<float>(kAtlasSize);
  }

  bool Font::Load(const std::string& key) {
    std::ifstream ifs(FilePath(key), std::ios::binary);
    if (!ifs) {
      return false;
    }
    fileData_.assign(std::istreambuf_iterator<char>(ifs), {});

    info_ = std::make_unique<stbtt_fontinfo>();
    const int offset = stbtt_GetFontOffsetForIndex(fileData_.data(), 0);
    if (offset < 0 || !stbtt_InitFont(info_.get(), fileData_.data(), offset)) {
      return false;
    }

    int ascent  = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(info_.get(), &ascent, &descent, &lineGap);
    emScale_ = stbtt_ScaleForMappingEmToPixels(info_.get(), 1.0f);
    ascent_  = static_cast<float>(ascent) * emScale_;
    descent_ = static_cast<float>(descent) * emScale_;
    lineGap_ = static_cast<float>(lineGap) * emScale_;

    DX12Basic*    dx12   = Renderer::GetInstance()->GetDX12Basic();
    ID3D12Device* device = dx12->GetDevice();

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = kAtlasSize;
    desc.Height           = kAtlasSize;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&atlas_.resource)))) {
      return false;
    }
    atlas_.state = D3D12_RESOURCE_STATE_COPY_DEST;
    atlas_->SetName(L"FontAtlas");

    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &uploadSize);
    uploadBuffer_ = dx12->MakeBufferResource(uploadSize);
    uploadBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pixels_));
    std::memset(pixels_, 0, uploadSize);

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    if (srvIndex_ == SrvManager::kInvalidIndex) {
      return false;
    }
    SrvManager::GetInstance()->CreateSRVForTexture2D(srvIndex_, atlas_.Get(), desc.Format, 1);

    // 字が 1 つも無くても、最初の Upload で読み取り状態へ移す
    isUploadNeeded_ = true;
    return true;
  }

  const Font::Glyph* Font::FindGlyph(char32_t codepoint) {
    if (const auto it = glyphs_.find(codepoint); it != glyphs_.end()) {
      return &it->second;
    }
    if (remainingNewGlyphs <= 0) {
      return nullptr;
    }

    Glyph glyph;
    glyph.index = stbtt_FindGlyphIndex(info_.get(), static_cast<int>(codepoint));

    int advance = 0;
    stbtt_GetGlyphHMetrics(info_.get(), glyph.index, &advance, nullptr);
    glyph.advance = static_cast<float>(advance) * emScale_;

    const GlyphSdf sdf = isFull_ ? GlyphSdf{} : MakeGlyphSdf(*info_, glyph.index, emScale_ * kBakeSize);
    if (!sdf.pixels.empty()) {
      --remainingNewGlyphs;

      if (penX_ + sdf.width + 1 > kAtlasSize) {
        penX_      = 1;
        penY_     += rowHeight_ + 1;
        rowHeight_ = 0;
      }
      // ponytail: アトラスは 1 枚だけ（全角で約 1100 字）。あふれた字は描かない。足りなくなったらページを増やす
      if (penY_ + sdf.height + 1 > kAtlasSize) {
        isFull_ = true;
        Logger::Log("Font: the glyph atlas is full. New characters are not drawn.\n");
      }
      else {
        for (int y = 0; y < sdf.height; ++y) {
          std::memcpy(pixels_ + static_cast<size_t>(penY_ + y) * footprint_.Footprint.RowPitch + penX_, sdf.pixels.data() + static_cast<size_t>(y) * sdf.width, static_cast<size_t>(sdf.width));
        }
        const float atlas = static_cast<float>(kAtlasSize);
        glyph.uvMin  = { static_cast<float>(penX_) / atlas, static_cast<float>(penY_) / atlas };
        glyph.uvMax  = { static_cast<float>(penX_ + sdf.width) / atlas, static_cast<float>(penY_ + sdf.height) / atlas };
        glyph.offset = { static_cast<float>(sdf.offsetX) / kBakeSize, static_cast<float>(sdf.offsetY) / kBakeSize };
        glyph.size   = { static_cast<float>(sdf.width) / kBakeSize, static_cast<float>(sdf.height) / kBakeSize };

        penX_          += sdf.width + 1;
        rowHeight_      = (std::max)(rowHeight_, sdf.height);
        isUploadNeeded_ = true;
      }
    }

    return &glyphs_.emplace(codepoint, glyph).first->second;
  }

  void Font::Upload() {
    if (!isUploadNeeded_) {
      return;
    }
    isUploadNeeded_ = false;

    DX12Basic* dx12 = Renderer::GetInstance()->GetDX12Basic();
    dx12->GetContext().Transition(atlas_, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource        = atlas_.Get();
    destination.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource       = uploadBuffer_.Get();
    source.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint_;

    // ponytail: 全面を送る（4MB、新しい字が出たフレームだけ）。重ければ足した行だけにする
    dx12->GetCommandList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    dx12->GetContext().Transition(atlas_, D3D12_RESOURCE_STATE_GENERIC_READ);
  }

  float Font::GetKerning(const Glyph& left, const Glyph& right) const {
    return static_cast<float>(stbtt_GetGlyphKernAdvance(info_.get(), left.index, right.index)) * emScale_;
  }

} // namespace Tako
