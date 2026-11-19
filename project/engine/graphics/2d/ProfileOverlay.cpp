#include "ProfileOverlay.h"

#include <format>
#include <span>
#include <string>
#include <string_view>

#include "FrameProfiler.h"
#include "FrameTimer.h"
#include "ShaderAsset.h"
#include "Text.h"
#include "WinApp.h"

namespace Tako {

  namespace {

    constexpr float   kTextSize     = 18.0f;      // px
    constexpr float   kOutlineWidth = 1.5f;       // px
    constexpr float   kOriginX      = 16.0f;      // 画面の左上から（px）
    constexpr float   kOriginY      = 16.0f;
    constexpr float   kNameWidth    = 260.0f;     // 名前の列の幅（px）
    constexpr float   kValueWidth   = 90.0f;      // 数字の列の幅（px）
    constexpr float   kFpsWidth     = 160.0f;     // FPS の箱の幅（px）
    constexpr int32_t kSortingOrder = 1'000'000;  // ゲームの UI より手前
    constexpr uint8_t kMaxDepth     = 1;          // これより深い区間は出さない

    std::unique_ptr<Text> MakeColumn(float x)
    {
      std::unique_ptr<Text> text = std::make_unique<Text>();
      text->SetPos({ kOriginX + x, kOriginY });
      text->SetOutline(Color(0.0f, 0.0f, 0.0f, 1.0f), kOutlineWidth);
      text->SetSortingOrder(kSortingOrder);
      text->GetMaterial().SetRenderQueue(ShaderAsset::kQueueOverlay);
      return text;
    }

  } // anonymous namespace

  ProfileOverlay::ProfileOverlay()
    : names_(MakeColumn(0.0f)), cpuValues_(MakeColumn(kNameWidth)), gpuValues_(MakeColumn(kNameWidth + kValueWidth)), fps_(MakeColumn(0.0f))
  {
  }

  ProfileOverlay::~ProfileOverlay() = default;

  void ProfileOverlay::Update(const FrameProfiler& profiler)
  {
    if (publishCount_ != profiler.GetPublishCount()) {
      publishCount_ = profiler.GetPublishCount();

      // 3 つの列は別々の文字列なので、行の数がずれないよう必ず 1 行ずつ足す
      std::string names;
      std::string cpu;
      std::string gpu;
      const auto addLine = [&](std::string_view name, std::string_view cpuText, std::string_view gpuText) {
        names.append(name).push_back('\n');
        cpu.append(cpuText).push_back('\n');
        gpu.append(gpuText).push_back('\n');
      };
      const auto ms = [](float value) { return std::format("{:.2f}", value); };

      const FrameTimes& times = profiler.GetFrameTimes();
      addLine("", "CPU ms", "GPU ms");
      addLine("CPU Update", ms(times.updateMs), "");
      addLine("CPU Render", ms(times.recordMs), "");
      addLine("GPU", "", ms(times.gpuMs));
      addLine("Wait for GPU", ms(times.gpuWaitMs), "");
      addLine("Idle", ms(times.idleMs), "");
      addLine("", "", "");

      const std::span<const ProfileSample> samples = profiler.GetSamples();
      for (size_t i = 0; i < samples.size(); ++i) {
        const ProfileSample& sample   = samples[i];
        const bool           hasChild = i + 1 < samples.size() && samples[i + 1].depth > sample.depth;
        // 中にパスの無いイベントは、見る物が無いので出さない
        if (sample.depth > kMaxDepth || (sample.isEvent && !hasChild)) {
          continue;
        }
        addLine(std::string(static_cast<size_t>(sample.depth) * 2, ' ') + sample.label, sample.isEvent ? std::string() : ms(sample.cpuMs),
                sample.hasGpuTime ? ms(sample.gpuMs) : std::string());
      }
      addLine("Other", "", ms(times.otherGpuMs));

      names_->SetLayout({ .string = names, .size = kTextSize, .wrap = false });
      cpuValues_->SetLayout({ .string = cpu, .size = kTextSize, .boxSize = { kValueWidth, 0.0f }, .align = { 1.0f, 0.0f }, .wrap = false });
      gpuValues_->SetLayout({ .string = gpu, .size = kTextSize, .boxSize = { kValueWidth, 0.0f }, .align = { 1.0f, 0.0f }, .wrap = false });
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
    names_->Update();
    cpuValues_->Update();
    gpuValues_->Update();
    fps_->Update();
  }

  void ProfileOverlay::Draw()
  {
    names_->Draw();
    cpuValues_->Draw();
    gpuValues_->Draw();
    fps_->Draw();
  }

} // namespace Tako
