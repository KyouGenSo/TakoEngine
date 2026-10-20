#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace Tako {

  /// <summary>
  /// D3D12 リソースと現在のリソース状態の組。状態は CommandContext::Transition が更新する
  /// </summary>
  struct GpuResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES                  state = D3D12_RESOURCE_STATE_COMMON;  ///< 記録中のコマンドリスト上での状態

    ID3D12Resource* Get() const { return resource.Get(); }
    ID3D12Resource* operator->() const { return resource.Get(); }
    explicit operator bool() const { return resource != nullptr; }

    /// <summary>
    /// リソースを解放し、状態を COMMON に戻す
    /// </summary>
    void Reset()
    {
      resource.Reset();
      state = D3D12_RESOURCE_STATE_COMMON;
    }
  };

} // namespace Tako
