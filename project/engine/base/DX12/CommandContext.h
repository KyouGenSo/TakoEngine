#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>

#include "GpuResource.h"

namespace Tako {

  /// <summary>
  /// コマンドリスト 1 本の記録窓口。バリアは溜めておき、GPU がリソースを触るコマンドの直前にまとめて発行する
  /// </summary>
  class CommandContext {
  public: //メンバー関数
    /// <summary>
    /// コマンドアロケータとコマンドリストを生成する。生成直後から記録できる
    /// </summary>
    /// <param name="device">D3D12 デバイス</param>
    void Initialize(ID3D12Device* device);

    /// <summary>
    /// 状態遷移を予約する。現在と同じ状態なら何もしない
    /// </summary>
    /// <param name="resource">対象リソース</param>
    /// <param name="newState">遷移後の状態</param>
    void Transition(GpuResource& resource, D3D12_RESOURCE_STATES newState);

    /// <summary>
    /// UAV 書き込みの完了待ちを予約する
    /// </summary>
    /// <param name="resource">対象リソース</param>
    void UavBarrier(GpuResource& resource);

    /// <summary>
    /// 予約済みのバリアを発行する
    /// </summary>
    void FlushBarriers();

    void Dispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ);

    void ExecuteIndirect(ID3D12CommandSignature* commandSignature, UINT maxCommandCount, GpuResource& argumentBuffer, UINT64 argumentOffset);

    /// <summary>
    /// 予約済みのバリアを発行してから記録を閉じる
    /// </summary>
    void Close();

    /// <summary>
    /// GPU の完了後に呼び、アロケータとコマンドリストを記録できる状態に戻す
    /// </summary>
    void Reset();

    //=========================================
    //Getter
    //=========================================
    /// <summary>
    /// 予約済みのバリアを発行してからコマンドリストを返す
    /// </summary>
    ID3D12GraphicsCommandList* GetCommandList();

  private: //メンバー変数
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<D3D12_RESOURCE_BARRIER>               pendingBarriers_;
  };

} // namespace Tako
