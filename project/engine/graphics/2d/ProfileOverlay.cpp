#include "ProfileOverlay.h"

#include <algorithm>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "FrameProfiler.h"
#include "FrameTimer.h"
#include "ShaderAsset.h"
#include "Text.h"
#include "WinApp.h"

namespace Tako {

  namespace {

    constexpr float   kTextSize       = 18.0f;                                  // px
    constexpr float   kOutlineWidth   = 1.5f;                                   // px
    constexpr float   kOriginX        = 16.0f;                                  // 画面の左上から（px）
    constexpr float   kOriginY        = 16.0f;
    constexpr float   kNameWidth      = 260.0f;                                 // 名前の列の幅（px）
    constexpr float   kValueWidth     = 90.0f;                                  // 数字の列の幅（px）
    constexpr float   kTableGap       = 40.0f;                                  // 左の表と右の表の間（px）
    constexpr float   kRenderTableX   = kNameWidth + kValueWidth + kTableGap;   // 右の表の左端
    constexpr float   kFpsWidth       = 160.0f;                                 // FPS の箱の幅（px）
    constexpr int32_t kSortingOrder   = 1'000'000;                              // ゲームの UI より手前
    constexpr uint8_t kMaxDepth       = 1;                                      // 描画の区間は、これより深い物を出さない
    constexpr size_t  kMaxNameLength  = 22;                                     // 名前はこのバイト数で切る（長いと数字の列に重なる）
    constexpr size_t  kMaxCpuChildren = 5;                                      // 更新の区間 1 つにつき、中の区間を時間の長い順にこの数まで出す

    std::unique_ptr<Text> MakeColumn(float x)
    {
      std::unique_ptr<Text> text = std::make_unique<Text>();
      text->SetPos({ kOriginX + x, kOriginY });
      text->SetOutline(Color(0.0f, 0.0f, 0.0f, 1.0f), kOutlineWidth);
      text->SetSortingOrder(kSortingOrder);
      text->GetMaterial().SetRenderQueue(ShaderAsset::kQueueOverlay);
      return text;
    }

    /// <summary>
    /// 深さの分だけ字下げした、表に出す名前
    /// </summary>
    std::string RowName(const ProfileSample& sample)
    {
      size_t length = (std::min)(sample.label.size(), kMaxNameLength);
      // UTF-8 の文字の途中で切らない
      while (length > 0 && length < sample.label.size() && (static_cast<unsigned char>(sample.label[length]) & 0xC0) == 0x80) {
        --length;
      }
      return std::string(static_cast<size_t>(sample.depth) * 2, ' ').append(sample.label, 0, length);
    }

    std::string Ms(float value)
    {
      return std::format("{:.2f}", value);
    }

  } // anonymous namespace

  ProfileOverlay::ProfileOverlay()
    : frameNames_(MakeColumn(0.0f))
    , frameValues_(MakeColumn(kNameWidth))
    , renderNames_(MakeColumn(kRenderTableX))
    , renderCpuValues_(MakeColumn(kRenderTableX + kNameWidth))
    , renderGpuValues_(MakeColumn(kRenderTableX + kNameWidth + kValueWidth))
    , fps_(MakeColumn(0.0f))
  {
  }

  ProfileOverlay::~ProfileOverlay() = default;

  void ProfileOverlay::Update(const FrameProfiler& profiler)
  {
    if (publishCount_ != profiler.GetPublishCount()) {
      publishCount_ = profiler.GetPublishCount();
      const FrameTimes& times = profiler.GetFrameTimes();

      // 1 つの表の列は別々の文字列なので、行の数がずれないよう必ず 1 行ずつ足す
      std::string frameNames;
      std::string frameValues;
      const auto addFrameLine = [&](std::string_view name, std::string_view value) {
        frameNames.append(name).push_back('\n');
        frameValues.append(value).push_back('\n');
      };
      addFrameLine("Frame", "ms");
      addFrameLine("CPU Update", Ms(times.updateMs));
      addFrameLine("CPU Render", Ms(times.recordMs));
      addFrameLine("GPU", Ms(times.gpuMs));
      addFrameLine("Wait for GPU", Ms(times.gpuWaitMs));
      addFrameLine("Idle", Ms(times.idleMs));
      addFrameLine("", "");

      // 更新の区間（CPU Update の内訳）。一番外の区間と、その直下の区間を出す
      addFrameLine("Update", "");
      const std::span<const ProfileSample> cpuSamples = profiler.GetCpuSamples();
      std::vector<const ProfileSample*>    children;
      for (size_t i = 0; i < cpuSamples.size();) {
        addFrameLine(RowName(cpuSamples[i]), Ms(cpuSamples[i].cpuMs));
        children.clear();
        for (++i; i < cpuSamples.size() && cpuSamples[i].depth > 0; ++i) {
          if (cpuSamples[i].depth == 1) {
            children.push_back(&cpuSamples[i]);
          }
        }
        // 型の多いゲームで縦に伸びないよう、時間の長い順に並べて頭だけ出す
        const size_t shownCount = (std::min)(children.size(), kMaxCpuChildren);
        std::ranges::partial_sort(children, children.begin() + static_cast<std::ptrdiff_t>(shownCount), std::ranges::greater{}, &ProfileSample::cpuMs);
        for (size_t child = 0; child < shownCount; ++child) {
          addFrameLine(RowName(*children[child]), Ms(children[child]->cpuMs));
        }
      }
      addFrameLine("Other", Ms(times.otherUpdateMs));

      std::string renderNames;
      std::string renderCpu;
      std::string renderGpu;
      const auto addRenderLine = [&](std::string_view name, std::string_view cpuText, std::string_view gpuText) {
        renderNames.append(name).push_back('\n');
        renderCpu.append(cpuText).push_back('\n');
        renderGpu.append(gpuText).push_back('\n');
      };
      addRenderLine("Render", "CPU ms", "GPU ms");
      const std::span<const ProfileSample> samples = profiler.GetSamples();
      for (size_t i = 0; i < samples.size(); ++i) {
        const ProfileSample& sample   = samples[i];
        const bool           hasChild = i + 1 < samples.size() && samples[i + 1].depth > sample.depth;
        // 中にパスの無いイベントは、見る物が無いので出さない
        if (sample.depth > kMaxDepth || (sample.isEvent && !hasChild)) {
          continue;
        }
        addRenderLine(RowName(sample), sample.isEvent ? std::string() : Ms(sample.cpuMs), sample.hasGpuTime ? Ms(sample.gpuMs) : std::string());
      }
      addRenderLine("Other", "", Ms(times.otherGpuMs));

      const auto setValues = [](Text& text, const std::string& values) {
        text.SetLayout({ .string = values, .size = kTextSize, .boxSize = { kValueWidth, 0.0f }, .align = { 1.0f, 0.0f }, .wrap = false });
      };
      frameNames_->SetLayout({ .string = frameNames, .size = kTextSize, .wrap = false });
      renderNames_->SetLayout({ .string = renderNames, .size = kTextSize, .wrap = false });
      setValues(*frameValues_, frameValues);
      setValues(*renderCpuValues_, renderCpu);
      setValues(*renderGpuValues_, renderGpu);
    }

    // FPS は 1 秒ごとの値（FrameTimer が表示用に持つ物）を、箱の右上を画面の右上に合わせて出す
    const float fps = FrameTimer::GetInstance()->GetDisplayFPS();
    if (fps != shownFps_) {
      shownFps_ = fps;
      fps_->SetLayout({ .string = std::format("FPS {:.1f}", fps), .size = kTextSize, .boxSize = { kFpsWidth, 0.0f }, .pivot = { 1.0f, 0.0f }, .align = { 1.0f, 0.0f },
                        .wrap = false });
    }
    fps_->SetPos({ static_cast<float>(WinApp::clientWidth) - kOriginX, kOriginY });

    // まだ作れていない字と、窓の大きさの変更に追従するため、文字が同じでも毎フレーム呼ぶ
    for (Text* text : { frameNames_.get(), frameValues_.get(), renderNames_.get(), renderCpuValues_.get(), renderGpuValues_.get(), fps_.get() }) {
      text->Update();
    }
  }

  void ProfileOverlay::Draw()
  {
    for (Text* text : { frameNames_.get(), frameValues_.get(), renderNames_.get(), renderCpuValues_.get(), renderGpuValues_.get(), fps_.get() }) {
      text->Draw();
    }
  }

} // namespace Tako
