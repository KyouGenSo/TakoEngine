#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <wrl.h>

namespace Tako {

  /// <summary>
  /// 1 フレーム内で同じ定数を視点ごとに別値で渡すための CB スロット列。フレーム先頭で Reset し、Push ごとに次のスロットを使う
  /// </summary>
  template<class T>
  class PerFrameConstantRing {
  private: //定数
    static constexpr size_t kSlotStride = (sizeof(T) + 255) & ~static_cast<size_t>(255);  ///< CBV のアドレス境界 256B に揃えたスロット幅

  public: //メンバー関数
    /// <summary>
    /// スロット数分のアップロードバッファを確保して常駐マップする
    /// </summary>
    /// <param name="device">バッファを生成するデバイス</param>
    /// <param name="slotCount">1 フレームで Push できる最大回数</param>
    void Initialize(ID3D12Device* device, uint32_t slotCount)
    {
      D3D12_HEAP_PROPERTIES heapProperties{};
      heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

      D3D12_RESOURCE_DESC resourceDesc{};
      resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      resourceDesc.Width = kSlotStride * slotCount;
      resourceDesc.Height = 1;
      resourceDesc.DepthOrArraySize = 1;
      resourceDesc.MipLevels = 1;
      resourceDesc.SampleDesc.Count = 1;
      resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      [[maybe_unused]] HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource_));
      assert(SUCCEEDED(hr));
      resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));

      slotCount_ = slotCount;
      nextSlot_ = 0;
    }

    /// <summary>
    /// 全スロットを空きに戻す。GPU は毎フレーム完了待ちされるため前フレームの内容は上書きしてよい
    /// </summary>
    void Reset() { nextSlot_ = 0; }

    /// <summary>
    /// 次の空きスロットへ書き込み、その GPU アドレスを返す
    /// </summary>
    /// <param name="data">書き込む定数</param>
    /// <returns>ルート CBV に渡す GPU 仮想アドレス</returns>
    D3D12_GPU_VIRTUAL_ADDRESS Push(const T& data)
    {
      assert(nextSlot_ < slotCount_ && "PerFrameConstantRing: slot exhausted");
      const size_t offset = kSlotStride * nextSlot_++;
      std::memcpy(mapped_ + offset, &data, sizeof(T));
      return resource_->GetGPUVirtualAddress() + offset;
    }

  private: //メンバー変数
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    uint8_t*                               mapped_    = nullptr;
    uint32_t                               slotCount_ = 0;
    uint32_t                               nextSlot_  = 0;
  };

} // namespace Tako
