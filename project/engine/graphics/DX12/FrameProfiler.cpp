#include "FrameProfiler.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

#include "CommandContext.h"

namespace Tako {

  namespace {

    float ToMs(std::chrono::steady_clock::duration duration)
    {
      return std::chrono::duration<float, std::milli>(duration).count();
    }

    float TicksToMs(uint64_t begin, uint64_t end, uint64_t frequency)
    {
      // 符号なしの引き算で巨大な値にならないよう、逆転していたら 0 にする
      if (end <= begin || frequency == 0) {
        return 0.0f;
      }
      return static_cast<float>(static_cast<double>(end - begin) * 1000.0 / static_cast<double>(frequency));
    }

  } // anonymous namespace

  void FrameProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, CommandContext* context)
  {
    context_     = context;
    commandList_ = context->GetCommandList();

    D3D12_QUERY_HEAP_DESC heapDesc{};
    heapDesc.Type  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = kQueryCount;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width            = kQueryCount * sizeof(uint64_t);
    bufferDesc.Height           = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels        = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    const bool isCreated = SUCCEEDED(queue->GetTimestampFrequency(&frequency_)) && frequency_ > 0 &&
                           SUCCEEDED(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_))) &&
                           SUCCEEDED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                                     IID_PPV_ARGS(&readback_)));
    if (!isCreated) {
      queryHeap_.Reset();
      readback_.Reset();
      frequency_ = 0;
    }
  }

  void FrameProfiler::BeginFrame()
  {
    if (isEnabled_ && !isEnabledRequested_) {
      rows_.clear();
      samples_.clear();
      cpuSamples_.clear();
      frameTimes_ = {};
    }
    if (!isEnabled_ && isEnabledRequested_) {
      ResetWindow(Clock::now());
    }
    isEnabled_  = isEnabledRequested_;
    scopeCount_ = 0;
    queryCount_ = 0;
    depth_      = 0;

    if (isEnabled_ && queryHeap_) {
      commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
      queryCount_ = 2;
    }
  }

  uint32_t FrameProfiler::BeginScope(std::string_view label, bool isEvent)
  {
    return OpenScope(label, isEvent ? ScopeKind::Event : ScopeKind::Gpu);
  }

  uint32_t FrameProfiler::BeginCpuScope(std::string_view label)
  {
    return OpenScope(label, ScopeKind::Cpu);
  }

  uint32_t FrameProfiler::OpenScope(std::string_view label, ScopeKind kind)
  {
    if (!isEnabled_) {
      return kInvalidScope;
    }
    if (scopeCount_ == scopes_.size()) {
      scopes_.emplace_back();
    }
    const uint32_t id    = scopeCount_++;
    Scope&         scope = scopes_[id];
    scope.label.assign(label);
    scope.depth     = depth_++;
    scope.kind      = kind;
    scope.query     = kNoQuery;
    scope.drawBegin = context_ ? context_->GetDrawCount() : 0;
    if (kind == ScopeKind::Gpu && queryHeap_ && queryCount_ + 2 <= kQueryCount) {
      scope.query = queryCount_;
      queryCount_ += 2;
      commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, scope.query);
    }
    scope.cpuBegin = Clock::now();
    return id;
  }

  void FrameProfiler::EndScope(uint32_t id, uint32_t count)
  {
    if (id == kInvalidScope) {
      return;
    }
    Scope& scope = scopes_[id];
    scope.cpuMs = ToMs(Clock::now() - scope.cpuBegin);
    scope.count = scope.kind == ScopeKind::Cpu ? count : (context_ ? context_->GetDrawCount() : 0) - scope.drawBegin;
    if (scope.query != kNoQuery) {
      commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, scope.query + 1);
    }
    --depth_;
  }

  void FrameProfiler::AddCpuSample(std::string_view label, float cpuMs, uint32_t count)
  {
    if (!isEnabled_) {
      return;
    }
    if (scopeCount_ == scopes_.size()) {
      scopes_.emplace_back();
    }
    Scope& scope = scopes_[scopeCount_++];
    scope.label.assign(label);
    scope.depth = depth_;
    scope.kind  = ScopeKind::Cpu;
    scope.query = kNoQuery;
    scope.cpuMs = cpuMs;
    scope.count = count;
  }

  void FrameProfiler::Resolve()
  {
    assert(depth_ == 0 && "FrameProfiler: a scope is still open at the end of the frame");
    if (queryCount_ == 0) {
      return;
    }
    commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    // 書いていない番号は解決しない
    commandList_->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, queryCount_, readback_.Get(), 0);
  }

  void FrameProfiler::ReadBack()
  {
    if (!isEnabled_) {
      return;
    }

    const uint64_t* ticks = nullptr;
    if (queryCount_ > 0) {
      const D3D12_RANGE readRange{ 0, queryCount_ * sizeof(uint64_t) };
      void*             mapped = nullptr;
      if (SUCCEEDED(readback_->Map(0, &readRange, &mapped))) {
        ticks = static_cast<const uint64_t*>(mapped);
      }
    }

    // 区間は毎フレーム同じ順に来るので、前のフレームまでの行を頭から順に当てていく
    size_t cursor = 0;
    for (uint32_t i = 0; i < scopeCount_; ++i) {
      const Scope& scope = scopes_[i];

      // 同じ親の下（深さが浅くなる手前まで）で、同じ名前の行を探す
      size_t found = cursor;
      while (found < rows_.size() && rows_[found].depth >= scope.depth && !(rows_[found].depth == scope.depth && rows_[found].label == scope.label)) {
        ++found;
      }
      if (found == rows_.size() || rows_[found].depth < scope.depth) {
        // 無ければ、手前の行の子孫の後ろに足す（途中に入れると、その子孫がこの行の子に見える）
        found = cursor;
        while (found < rows_.size() && rows_[found].depth > scope.depth) {
          ++found;
        }
        rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(found), Row{ .label = scope.label, .depth = scope.depth });
      }

      Row& row = rows_[found];
      row.kind   = scope.kind;
      row.cpuMs += scope.cpuMs;
      row.count += scope.count;
      ++row.frames;
      if (ticks && scope.query != kNoQuery) {
        row.gpuMs     += TicksToMs(ticks[scope.query], ticks[scope.query + 1], frequency_);
        row.hasGpuTime = true;
      }
      cursor = found + 1;
    }

    if (ticks) {
      gpuSum_ += TicksToMs(ticks[0], ticks[1], frequency_);
      const D3D12_RANGE noWrite{ 0, 0 };
      readback_->Unmap(0, &noWrite);
    }

    ++windowFrames_;
    const Clock::time_point now = Clock::now();
    if (windowFrames_ >= minWindowFrames_ && now - windowStart_ >= windowLength_) {
      Publish(now);
    }
  }

  void FrameProfiler::AddCpuTimes(float updateMs, float drawMs)
  {
    if (!isEnabled_) {
      return;
    }
    updateSum_ += updateMs;
    drawSum_   += drawMs;
    ++cpuFrames_;
  }

  void FrameProfiler::AddWaitTimes(float gpuWaitMs, float idleMs)
  {
    if (!isEnabled_) {
      return;
    }
    gpuWaitSum_ += gpuWaitMs;
    idleSum_    += idleMs;
    ++waitFrames_;
  }

  void FrameProfiler::SetWindow(float seconds, uint32_t minFrames)
  {
    windowLength_    = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(seconds));
    minWindowFrames_ = (std::max)(minFrames, 1u);
  }

  void FrameProfiler::Publish(Clock::time_point now)
  {
    const float perFrame = 1.0f / static_cast<float>(windowFrames_);

    samples_.clear();
    cpuSamples_.clear();
    float accountedGpuMs    = 0.0f;
    float accountedUpdateMs = 0.0f;
    int   timedDepth        = -1;     // GPU 時間を持つ行の中にいる間の、その行の深さ
    bool  isCpuTree         = false;  // 一番外が CPU だけの区間の、その中にいる間
    for (const Row& row : rows_) {
      if (row.frames == 0) {
        continue;
      }
      if (row.depth == 0) {
        isCpuTree = row.kind == ScopeKind::Cpu;
      }
      ProfileSample sample{ row.label, static_cast<float>(row.count) * perFrame, row.cpuMs * perFrame, row.gpuMs * perFrame, row.depth, row.kind == ScopeKind::Event,
                            row.hasGpuTime };
      if (isCpuTree) {
        if (row.depth == 0) {
          accountedUpdateMs += sample.cpuMs;
        }
        cpuSamples_.push_back(std::move(sample));
        continue;
      }
      samples_.push_back(std::move(sample));

      // 内側の区間は外側の時間に含まれるので、一番外の区間だけを足す
      if (timedDepth >= 0 && row.depth <= timedDepth) {
        timedDepth = -1;
      }
      if (row.hasGpuTime && timedDepth < 0) {
        accountedGpuMs += row.gpuMs * perFrame;
        timedDepth      = row.depth;
      }
    }

    const float drawMs        = cpuFrames_ > 0 ? drawSum_ / static_cast<float>(cpuFrames_) : 0.0f;
    frameTimes_.updateMs      = cpuFrames_ > 0 ? updateSum_ / static_cast<float>(cpuFrames_) : 0.0f;
    frameTimes_.otherUpdateMs = (std::max)(frameTimes_.updateMs - accountedUpdateMs, 0.0f);
    frameTimes_.gpuWaitMs     = waitFrames_ > 0 ? gpuWaitSum_ / static_cast<float>(waitFrames_) : 0.0f;
    frameTimes_.idleMs        = waitFrames_ > 0 ? idleSum_ / static_cast<float>(waitFrames_) : 0.0f;
    frameTimes_.recordMs      = (std::max)(drawMs - frameTimes_.gpuWaitMs - frameTimes_.idleMs, 0.0f);
    frameTimes_.gpuMs         = gpuSum_ * perFrame;
    frameTimes_.otherGpuMs    = (std::max)(frameTimes_.gpuMs - accountedGpuMs, 0.0f);

    ++publishCount_;
    std::erase_if(rows_, [](const Row& row) { return row.frames == 0; });
    ResetWindow(now);
  }

  void FrameProfiler::ResetWindow(Clock::time_point now)
  {
    for (Row& row : rows_) {
      row.cpuMs      = 0.0f;
      row.gpuMs      = 0.0f;
      row.count      = 0;
      row.frames     = 0;
      row.hasGpuTime = false;
    }
    windowStart_  = now;
    windowFrames_ = 0;
    cpuFrames_    = 0;
    waitFrames_   = 0;
    gpuSum_       = 0.0f;
    updateSum_    = 0.0f;
    drawSum_      = 0.0f;
    gpuWaitSum_   = 0.0f;
    idleSum_      = 0.0f;
  }

#ifdef _DEBUG
  void RunFrameProfilerSelfTest()
  {
    assert(TicksToMs(0, 10'000'000, 10'000'000) == 1000.0f);
    assert(TicksToMs(5, 3, 1000) == 0.0f);
    assert(TicksToMs(0, 1, 0) == 0.0f);

    // デバイスなし（GPU 時間なし）で、2 フレームを 1 つの期間にして確かめる
    FrameProfiler profiler;
    profiler.SetEnabled(true);
    profiler.SetWindow(0.0f, 2);

    const auto runFrame = [&profiler](bool hasFarCascade) {
      profiler.BeginFrame();
      const uint32_t components = profiler.BeginCpuScope("Components");
      profiler.AddCpuSample("Mesh", 0.5f, 3);
      profiler.EndScope(components, 3);
      profiler.EndScope(profiler.BeginCpuScope("Collision"));
      const uint32_t shadows = profiler.BeginScope("Shadows");
      profiler.EndScope(profiler.BeginScope("Cascade 0"));
      if (hasFarCascade) {
        profiler.EndScope(profiler.BeginScope("Cascade 2"));
      }
      profiler.EndScope(shadows);
      const uint32_t event = profiler.BeginScope("AfterOpaque", true);
      profiler.EndScope(profiler.BeginScope("Pass"));
      profiler.EndScope(event);
      profiler.Resolve();
      profiler.ReadBack();
    };

    // 1 フレームおきの区間も、期間の中で 1 回通れば行が残る。並びは開いた順で、後ろの行は飛ばした行の先で見つかる
    runFrame(true);
    assert(profiler.GetSamples().empty());
    runFrame(false);
    std::span<const ProfileSample> samples = profiler.GetSamples();
    assert(samples.size() == 5);
    assert(samples[0].label == "Shadows" && samples[0].depth == 0 && !samples[0].isEvent);
    assert(samples[1].label == "Cascade 0" && samples[1].depth == 1);
    assert(samples[2].label == "Cascade 2" && samples[2].depth == 1);
    assert(samples[3].label == "AfterOpaque" && samples[3].depth == 0 && samples[3].isEvent);
    assert(samples[4].label == "Pass" && samples[4].depth == 1 && !samples[4].hasGpuTime);

    // CPU だけの区間は別の一覧に出る。足した時間は開いている区間の子になり、個数は 1 フレームあたりの平均になる
    std::span<const ProfileSample> cpuSamples = profiler.GetCpuSamples();
    assert(cpuSamples.size() == 3);
    assert(cpuSamples[0].label == "Components" && cpuSamples[0].depth == 0 && cpuSamples[0].count == 3.0f);
    assert(cpuSamples[1].label == "Mesh" && cpuSamples[1].depth == 1 && cpuSamples[1].cpuMs == 0.5f && cpuSamples[1].count == 3.0f);
    assert(cpuSamples[2].label == "Collision" && cpuSamples[2].depth == 0 && cpuSamples[2].count == 0.0f && !cpuSamples[2].hasGpuTime);

    // 期間の中で 1 回も通らなかった行は消える
    runFrame(false);
    runFrame(false);
    samples = profiler.GetSamples();
    assert(samples.size() == 4 && samples[2].label == "AfterOpaque" && samples[3].label == "Pass");

    // 途中に増えた区間は、手前の行の子孫の後ろに入る
    profiler.BeginFrame();
    const uint32_t shadows = profiler.BeginScope("Shadows");
    profiler.EndScope(profiler.BeginScope("Cascade 0"));
    profiler.EndScope(shadows);
    profiler.EndScope(profiler.BeginScope("Opaque"));
    profiler.EndScope(profiler.BeginScope("AfterOpaque", true));
    profiler.Resolve();
    profiler.ReadBack();
    runFrame(false);
    samples = profiler.GetSamples();
    assert(samples.size() == 5 && samples[2].label == "Opaque" && samples[2].depth == 0 && samples[3].label == "AfterOpaque");

    // 無効にすると、区間は開かず、結果も空になる
    profiler.SetEnabled(false);
    profiler.BeginFrame();
    assert(profiler.BeginScope("Shadows") == FrameProfiler::kInvalidScope);
    assert(profiler.BeginCpuScope("Components") == FrameProfiler::kInvalidScope);
    assert(profiler.GetSamples().empty() && profiler.GetCpuSamples().empty());
  }
#endif

} // namespace Tako
