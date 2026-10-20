#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <wrl.h>

namespace Tako {

  /// <summary>
  /// 1 フレームだけ使う定数を先頭から詰めていく常駐マップのアップロードバッファ
  /// </summary>
  class UploadAllocator {
  public: //定数
    static constexpr size_t kConstantAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

  public: //構造体
    /// <summary>
    /// 確保した領域の CPU 書き込み先と GPU アドレス
    /// </summary>
    struct Allocation {
      void*                     cpu = nullptr;
      D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    };

  public: //メンバー関数
    /// <summary>
    /// アップロードバッファを確保して常駐マップする
    /// </summary>
    /// <param name="device">バッファを生成するデバイス</param>
    /// <param name="capacity">1 フレームで確保できる総バイト数</param>
    void Initialize(ID3D12Device* device, size_t capacity)
    {
      D3D12_HEAP_PROPERTIES heapProperties{};
      heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

      D3D12_RESOURCE_DESC resourceDesc{};
      resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      resourceDesc.Width = capacity;
      resourceDesc.Height = 1;
      resourceDesc.DepthOrArraySize = 1;
      resourceDesc.MipLevels = 1;
      resourceDesc.SampleDesc.Count = 1;
      resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      [[maybe_unused]] HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource_));
      assert(SUCCEEDED(hr));
      resource_->SetName(L"UploadAllocator");
      resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));

      gpuBase_ = resource_->GetGPUVirtualAddress();
      capacity_ = capacity;
      offset_ = 0;
    }

    /// <summary>
    /// 全領域を空きに戻す。GPU の完了を待った後に呼ぶ
    /// </summary>
    void Reset() { offset_ = 0; }

    /// <summary>
    /// 次の空き領域を確保する
    /// </summary>
    /// <param name="size">確保するバイト数</param>
    /// <param name="alignment">先頭アドレスの境界（CBV は 256）</param>
    Allocation Allocate(size_t size, size_t alignment = kConstantAlignment)
    {
      const size_t begin = (offset_ + alignment - 1) & ~(alignment - 1);
      assert(begin + size <= capacity_ && "UploadAllocator: capacity exhausted");
      offset_ = begin + size;
      return { mapped_ + begin, gpuBase_ + begin };
    }

    /// <summary>
    /// 定数を 1 つ書き込み、その GPU アドレスを返す
    /// </summary>
    template<class T>
    D3D12_GPU_VIRTUAL_ADDRESS Push(const T& data)
    {
      const Allocation allocation = Allocate(sizeof(T));
      std::memcpy(allocation.cpu, &data, sizeof(T));
      return allocation.gpu;
    }

  private: //メンバー変数
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    uint8_t*                               mapped_   = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS              gpuBase_  = 0;
    size_t                                 capacity_ = 0;
    size_t                                 offset_   = 0;
  };

} // namespace Tako
