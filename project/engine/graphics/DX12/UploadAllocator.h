#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

namespace Tako {

  /// <summary>
  /// 1 フレームだけ使う定数を先頭から詰めていく常駐マップのアップロードバッファ。
  /// ページが尽きたら次のページへ移り、無ければ足す（足したページは以後のフレームでも使い回す）
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
    /// 最初のページを確保して常駐マップする
    /// </summary>
    /// <param name="device">ページのバッファを生成するデバイス</param>
    /// <param name="pageSize">1 ページのバイト数（1 回に確保できる上限でもある）</param>
    void Initialize(ID3D12Device* device, size_t pageSize)
    {
      device_   = device;
      pageSize_ = pageSize;
      pages_.clear();
      AddPage();
      Reset();
    }

    /// <summary>
    /// 全ページを空きに戻す。GPU の完了を待った後に呼ぶ
    /// </summary>
    void Reset()
    {
      page_   = 0;
      offset_ = 0;
    }

    /// <summary>
    /// 次の空き領域を確保する
    /// </summary>
    /// <param name="size">確保するバイト数</param>
    /// <param name="alignment">先頭アドレスの境界（CBV は 256）</param>
    Allocation Allocate(size_t size, size_t alignment = kConstantAlignment)
    {
      assert(size <= pageSize_ && "UploadAllocator: allocation larger than a page");
      size_t begin = (offset_ + alignment - 1) & ~(alignment - 1);
      if (begin + size > pageSize_) {
        // 今のページは記録済みのコマンドが読むので、そのまま残して次のページから詰める
        if (++page_ == pages_.size()) {
          AddPage();
        }
        begin = 0;
      }
      offset_ = begin + size;
      const Page& page = pages_[page_];
      return { page.mapped + begin, page.gpu + begin };
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

  private: //構造体
    struct Page {
      Microsoft::WRL::ComPtr<ID3D12Resource> resource;
      uint8_t*                               mapped = nullptr;
      D3D12_GPU_VIRTUAL_ADDRESS              gpu    = 0;
    };

  private: //非公開関数
    void AddPage()
    {
      D3D12_HEAP_PROPERTIES heapProperties{};
      heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

      D3D12_RESOURCE_DESC resourceDesc{};
      resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      resourceDesc.Width = pageSize_;
      resourceDesc.Height = 1;
      resourceDesc.DepthOrArraySize = 1;
      resourceDesc.MipLevels = 1;
      resourceDesc.SampleDesc.Count = 1;
      resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      Page& page = pages_.emplace_back();
      [[maybe_unused]] HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&page.resource));
      assert(SUCCEEDED(hr));
      page.resource->SetName(L"UploadAllocator");
      page.resource->Map(0, nullptr, reinterpret_cast<void**>(&page.mapped));
      page.gpu = page.resource->GetGPUVirtualAddress();
    }

  private: //メンバー変数
    ID3D12Device*     device_   = nullptr;
    std::vector<Page> pages_;
    size_t            pageSize_ = 0;
    size_t            page_     = 0;  ///< 今詰めているページ
    size_t            offset_   = 0;  ///< page_ の中の次の空き
  };

} // namespace Tako
