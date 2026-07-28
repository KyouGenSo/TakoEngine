#include "GPUParticle.h"
#include "GPUParticleEmitter.h"
#include "SphereEmitter.h"
#include "BoxEmitter.h"
#include "TriangleEmitter.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "TextureManager.h"
#include "FrameTimer.h"
#include "WinApp.h"
#include "EnginePaths.h"
#include "Model.h"
#include "Mesh.h"
#include "ModelManager.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "DebugCamera.h"
#endif // _DEBUG

#include <numbers>

namespace Tako {

  namespace {
    // 各ルートシグネチャのルートパラメータ番号
    namespace InitCsRP {
      constexpr UINT kParticleUavParam      = 0;  ///< u0: パーティクル
      constexpr UINT kFreeListIndexUavParam = 1;  ///< u1: フリーリスト先頭 index
      constexpr UINT kFreeListUavParam      = 2;  ///< u2: フリーリスト
    }
    namespace EmitCsRP {
      constexpr UINT kParticleUavParam         = 0;  ///< u0: パーティクル
      constexpr UINT kEmitterSrvParam          = 1;  ///< t0: エミッター
      constexpr UINT kPerFrameCbvParam         = 2;  ///< b0: フレーム情報
      constexpr UINT kFreeListIndexUavParam    = 3;  ///< u1: フリーリスト先頭 index
      constexpr UINT kFreeListUavParam         = 4;  ///< u2: フリーリスト
      constexpr UINT kMeshVertexParam          = 5;  ///< t10: メッシュ頂点
      constexpr UINT kMeshIndexParam           = 6;  ///< t11: メッシュインデックス
      constexpr UINT kMeshAreaPrefixSumParam   = 7;  ///< t12: メッシュ面積プレフィックスサム
      constexpr UINT kTargetMeshEmitterIdParam = 8;  ///< b1: 対象メッシュエミッター ID（32bit 定数）
    }
    namespace ResetCountersRP {
      constexpr UINT kPerEmitterCountUavParam = 0;  ///< u0: per-emitter 生存数
    }
    namespace IntegrateRP {
      constexpr UINT kParticleUavParam        = 0;  ///< u0: パーティクル
      constexpr UINT kFreeListIndexUavParam   = 1;  ///< u1: フリーリスト先頭 index
      constexpr UINT kFreeListUavParam        = 2;  ///< u2: フリーリスト
      constexpr UINT kForceFieldSrvParam      = 3;  ///< t0: フォースフィールド
      constexpr UINT kDepthSrvParam           = 4;  ///< t1: 深度バッファ
      constexpr UINT kPerFrameCbvParam        = 5;  ///< b0: フレーム情報
      constexpr UINT kPhysicsParamsCbvParam   = 6;  ///< b1: 物理パラメータ
      constexpr UINT kEmitterSrvParam         = 7;  ///< t2: エミッター
      constexpr UINT kPerEmitterCountUavParam = 8;  ///< u3: per-emitter 生存数
    }
    namespace BuildDrawArgsRP {
      constexpr UINT kPerEmitterCountUavParam   = 0;  ///< u0: per-emitter 生存数
      constexpr UINT kDrawArgsUavParam          = 1;  ///< u1: Indirect 描画引数
      constexpr UINT kScatterCursorUavParam     = 2;  ///< u2: スキャッタ用カーソル
      constexpr UINT kEmitterIndexCountSrvParam = 3;  ///< t0: エミッターごとの index 数
    }
    namespace ScatterCompactRP {
      constexpr UINT kParticleUavParam      = 0;  ///< u0: パーティクル
      constexpr UINT kDrawIndexUavParam     = 1;  ///< u1: 描画 index リスト
      constexpr UINT kScatterCursorUavParam = 2;  ///< u2: スキャッタ用カーソル
    }
    namespace DrawRP {
      constexpr UINT kParticleSrvParam    = 0;  ///< t0 (VS): パーティクル
      constexpr UINT kPerViewCbvParam     = 1;  ///< b0 (VS): ビュー情報
      constexpr UINT kTextureParam        = 2;  ///< t0 (PS): テクスチャ
      constexpr UINT kEmitterSrvParam     = 3;  ///< t2 (VS): エミッター（ビルボード判定）
      constexpr UINT kModelVertexParam    = 4;  ///< t3 (VS): 描画モデル頂点
      constexpr UINT kModelIndexParam     = 5;  ///< t4 (VS): 描画モデルインデックス
    }
  }

  std::unique_ptr<GPUParticle> GPUParticle::instance_ = nullptr;

  const uint32_t GPUParticle::kNumMaxParticle = 1000000;

  const uint32_t GPUParticle::kNumMaxEmitter = 1000;

  GPUParticle* GPUParticle::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<GPUParticle>(Token{});
    }
    return instance_.get();
  }

  void GPUParticle::Initialize(DX12Basic* dx12, Camera* camera)
  {
    dx12_ = dx12;
    camera_ = camera;
    srvManager_ = SrvManager::GetInstance();

    modelData_.textureData.texturePath = "circle.dds";
    modelData_.textureData.textureIndex = TextureManager::GetInstance()->GetEngineDefaultSRVIndex(modelData_.textureData.texturePath);

    isInited_ = false;

    isDebug_ = false;

    // ルートシグネチャの生成
    CreateRS();
    CreateInitComputeRS();
    CreateEmitParticleComputeRS();
    CreateIntegrateAllComputeRS();
    CreateResetCountersRS();
    CreateBuildDrawArgsRS();
    CreateScatterCompactRS();

    // PSO の生成 (ブレンドモード別に 3 つ生成)
    {
      // 加算 (発光・火花向け): src*srcA + dst
      D3D12_BLEND_DESC blendAdd{};
      blendAdd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
      blendAdd.RenderTarget[0].BlendEnable = true;
      blendAdd.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      blendAdd.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
      blendAdd.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      blendAdd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
      blendAdd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
      blendAdd.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

      // スクリーン (発光感を保ちつつ白飛び抑制): src + dst - src*dst
      D3D12_BLEND_DESC blendScreen = blendAdd;
      blendScreen.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
      blendScreen.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

      // アルファ合成: src*srcA + dst*(1-srcA)
      D3D12_BLEND_DESC blendAlpha = blendAdd;
      blendAlpha.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      blendAlpha.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

      // 全パーティクル共通の描画 PSO
      CreateDrawPSO(blendAdd, psoAdd_);
      CreateDrawPSO(blendScreen, psoScreen_);
      CreateDrawPSO(blendAlpha, psoAlpha_);
    }
    CreateComputeShaderPSO(initComputeRS_, initComputePSO_, L"InitParticle.CS.hlsl");
    CreateComputeShaderPSO(emitParticleRS_, emitParticlePSO_, L"EmitParticle.CS.hlsl");
    CreateComputeShaderPSO(integrateAllRS_, integrateAllPSO_, L"IntegrateAll.CS.hlsl");
    CreateComputeShaderPSO(resetCountersRS_, resetCountersPSO_, L"ResetCounters.CS.hlsl");
    CreateComputeShaderPSO(buildDrawArgsRS_, buildDrawArgsPSO_, L"BuildDrawArgs.CS.hlsl");
    CreateComputeShaderPSO(scatterCompactRS_, scatterCompactPSO_, L"ScatterCompact.CS.hlsl");

    // PerView データの生成
    CreatePerViewData();

    // PerFrame データの生成
    CreatePerFrameData();

    // パーティクルリソースの生成
    CreateParticleResource();

    // EmitterSphere データの生成
    CreateEmitterData();

    // デフォルトの描画モデル (板ポリ) の生成
    CreateDefaultQuadMesh();

    // FreeCounter リソースの生成
    CreateFreeListResource();

    // Indirect 描画・コンパクション用リソースの生成
    CreateIndirectResources();
    CreateCommandSignature();

#ifdef _DEBUG
    // Readback バッファの生成（アクティブパーティクル数取得用）
    CreateFreeListReadbackResource();
#endif

    // フォースフィールドリソースの生成
    CreateForceFieldResource();

    // 物理パラメータリソースの生成
    CreatePhysicsParamsResource();

    // 深度バッファ用 SRV の作成
    CreateDepthSRV();
  }

  void GPUParticle::Update()
  {
    // PerFrame の更新
    UpdatePerFrame();

    // エミッターの更新
    UpdateEmitter();

    // PerView の更新
    UpdatePerView();

    SyncEmitterData();

    // 物理パラメータの更新
    UpdatePhysicsParams();

    // フォースフィールドデータの同期
    SyncForceFieldData();
  }

  void GPUParticle::Draw()
  {
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

    /// ================================== ///
    ///            ComputerShader          ///
    /// ================================== ///

    //--------------------------------------初期化--------------------------------------//
    if (!isInited_) {
      // ルートシグネチャの設定
      commandList->SetComputeRootSignature(initComputeRS_.Get());

      // パイプラインステートの設定
      commandList->SetPipelineState(initComputePSO_.Get());

      // ParticleData の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(InitCsRP::kParticleUavParam, particleUavIndex_);

      // FreeListIndex の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(InitCsRP::kFreeListIndexUavParam, freeListIndexUavIndex_);

      // FreeList の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(InitCsRP::kFreeListUavParam, freeListUavIndex_);

      // ディスパッチ
      commandList->Dispatch(1024, 1, 1);

      isInited_ = true;
    }

    // リソースバリアの設定（UAV 同期）
    dx12_->SetUAVBarrier(particleResource_.Get());
    dx12_->SetUAVBarrier(freeListIndexResource_.Get());
    dx12_->SetUAVBarrier(freeListResource_.Get());

    //--------------------------------------射出--------------------------------------//
      // アクティブなエミッターがある場合のみ実行
    if (!activeEmitters_.empty()) {
      // ルートシグネチャの設定
      commandList->SetComputeRootSignature(emitParticleRS_.Get());

      // パイプラインステートの設定
      commandList->SetPipelineState(emitParticlePSO_.Get());

      // ParticleData の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kParticleUavParam, particleUavIndex_);

      // FreeListIndex の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kFreeListIndexUavParam, freeListIndexUavIndex_);

      // FreeList の UAV の設定
      srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kFreeListUavParam, freeListUavIndex_);

      // エミッターリストの SRV の設定
      srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kEmitterSrvParam, emitterSrvIndex_);

      commandList->SetComputeRootConstantBufferView(EmitCsRP::kPerFrameCbvParam, perFrameResource_->GetGPUVirtualAddress());

      bool hasNonMeshEmitter = false;
      for (const auto& emitter : activeEmitters_) {
        if (emitter && emitter->GetType() != EmitterType::Mesh) {
          hasNonMeshEmitter = true;
          break;
        }
      }

      // 非 Mesh エミッタを一括処理 (Mesh SRV はダミー bind、HLSL 側で Mesh タイプは早期 return)
      if (hasNonMeshEmitter) {
        const uint32_t threadGroupsX = (static_cast<uint32_t>(activeEmitters_.size()) + 15) / 16;
        commandList->SetComputeRoot32BitConstant(EmitCsRP::kTargetMeshEmitterIdParam, kInvalidMeshTarget, 0);
        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshVertexParam, emitterSrvIndex_);
        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshIndexParam, emitterSrvIndex_);
        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshAreaPrefixSumParam, emitterSrvIndex_);
        commandList->Dispatch(threadGroupsX, 1, 1);
      }

      // Mesh エミッタを 1 つずつ別 Dispatch (スキニング有効なら skinned SRV を優先)。
      // HLSL 側はスレッド 0 が gTargetMeshEmitterId のエミッタを担当するため 1 グループで足りる
      for (uint32_t i = 0; i < activeEmitters_.size(); ++i) {
        const auto& emitter = activeEmitters_[i];
        if (!emitter || emitter->GetType() != EmitterType::Mesh) continue;

        const auto& edata = emitter->GetData();
        const uint32_t vtxSrv = (edata.meshSkinnedVertexSrvIndex != 0)
          ? edata.meshSkinnedVertexSrvIndex
          : edata.meshVertexSrvIndex;

        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshVertexParam,
          vtxSrv != 0 ? vtxSrv : emitterSrvIndex_);
        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshIndexParam,
          edata.meshIndexSrvIndex != 0 ? edata.meshIndexSrvIndex : emitterSrvIndex_);
        srvManager_->SetComputeRootDescriptorTable(EmitCsRP::kMeshAreaPrefixSumParam,
          edata.meshAreaPrefixSumSrvIndex != 0 ? edata.meshAreaPrefixSumSrvIndex : emitterSrvIndex_);
        commandList->SetComputeRoot32BitConstant(EmitCsRP::kTargetMeshEmitterIdParam, i, 0);
        commandList->Dispatch(1, 1, 1);
      }
    }

    // リソースバリアの設定（UAV 同期）
    dx12_->SetUAVBarrier(particleResource_.Get());
    dx12_->SetUAVBarrier(freeListIndexResource_.Get());
    dx12_->SetUAVBarrier(freeListResource_.Get());

    //--------------------------------------per-emitter 生存数カウンタのリセット--------------------------------------//
    // IntegrateAll が InterlockedAdd で加算する前に perEmitterCount[0..kNumMaxEmitter) を 0 クリアする
    commandList->SetComputeRootSignature(resetCountersRS_.Get());
    commandList->SetPipelineState(resetCountersPSO_.Get());
    srvManager_->SetComputeRootDescriptorTable(ResetCountersRP::kPerEmitterCountUavParam, perEmitterCountUavIndex_);
    commandList->Dispatch(1, 1, 1); // numthreads(1024) で kNumMaxEmitter をカバー
    dx12_->SetUAVBarrier(perEmitterCountResource_.Get());

    //--------------------------------------IntegrateAll--------------------------------------//

    // 深度バッファを NON_PIXEL_SHADER_RESOURCE に遷移（深度衝突用）
    dx12_->TransitionResourceWithTracking(
      dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // ルートシグネチャの設定
    commandList->SetComputeRootSignature(integrateAllRS_.Get());

    // パイプラインステートの設定
    commandList->SetPipelineState(integrateAllPSO_.Get());

    // ParticleData の UAV の設定 (u0)
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kParticleUavParam, particleUavIndex_);

    // FreeListIndex の UAV の設定 (u1)
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kFreeListIndexUavParam, freeListIndexUavIndex_);

    // FreeList の UAV の設定 (u2)
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kFreeListUavParam, freeListUavIndex_);

    // ForceFields の SRV の設定 (t0)
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kForceFieldSrvParam, forceFieldSrvIndex_);

    // DepthBuffer の SRV の設定 (t1) — 深度バッファ衝突用
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kDepthSrvParam, depthSrvIndex_);

    // PerFrame の CBV の設定 (b0)
    commandList->SetComputeRootConstantBufferView(IntegrateRP::kPerFrameCbvParam, perFrameResource_->GetGPUVirtualAddress());

    // PhysicsParams の CBV の設定 (b1)
    commandList->SetComputeRootConstantBufferView(IntegrateRP::kPhysicsParamsCbvParam, physicsParamsResource_->GetGPUVirtualAddress());

    // Emitter SRV の設定 (t2) — IntegrateAll が targetPosition 等を参照
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kEmitterSrvParam, emitterSrvIndex_);

    // perEmitterCount UAV の設定 (u3) — per-emitter 生存数のカウント先
    srvManager_->SetComputeRootDescriptorTable(IntegrateRP::kPerEmitterCountUavParam, perEmitterCountUavIndex_);

    // ディスパッチ（256スレッド/グループ × ceil(1M/256) = 3907グループ）
    uint32_t integrateGroups = (kNumMaxParticle + 255) / 256;
    commandList->Dispatch(integrateGroups, 1, 1);

    dx12_->SetUAVBarrier(particleResource_.Get());
    dx12_->SetUAVBarrier(perEmitterCountResource_.Get()); // BuildDrawArgs が読む前に per-emitter 生存数の書き込みを同期

#ifdef _DEBUG
    // アクティブパーティクル数の Readback
    ReadbackActiveParticleCount();
#endif

    // 深度バッファを DEPTH_WRITE に復帰
    dx12_->TransitionResourceWithTracking(
      dx12_->GetDepthStencilResource(),
      D3D12_RESOURCE_STATE_DEPTH_WRITE);

    //--------------------------------------BuildDrawArgs--------------------------------------//
    // per-emitter 生存数を排他プレフィックスサムし、per-emitter の Indirect 引数(base_e/count_e) と
    // スキャッタ用カーソル(=base_e) を構築する
    commandList->SetComputeRootSignature(buildDrawArgsRS_.Get());
    commandList->SetPipelineState(buildDrawArgsPSO_.Get());
    srvManager_->SetComputeRootDescriptorTable(BuildDrawArgsRP::kPerEmitterCountUavParam, perEmitterCountUavIndex_);
    srvManager_->SetComputeRootDescriptorTable(BuildDrawArgsRP::kDrawArgsUavParam, drawArgsUavIndex_);
    srvManager_->SetComputeRootDescriptorTable(BuildDrawArgsRP::kScatterCursorUavParam, scatterCursorUavIndex_);
    srvManager_->SetComputeRootDescriptorTable(BuildDrawArgsRP::kEmitterIndexCountSrvParam, emitterIndexCountSrvIndex_);
    commandList->Dispatch(1, 1, 1);
    dx12_->SetUAVBarrier(drawArgsResource_.Get());
    dx12_->SetUAVBarrier(scatterCursorResource_.Get());

    //--------------------------------------ScatterCompact--------------------------------------//
    // 散在する生存パーティクル index を drawIndexList に先頭から詰め直す
    // (particleResource_ は IntegrateAll 直後の UAV state のまま読む)
    commandList->SetComputeRootSignature(scatterCompactRS_.Get());
    commandList->SetPipelineState(scatterCompactPSO_.Get());
    srvManager_->SetComputeRootDescriptorTable(ScatterCompactRP::kParticleUavParam, particleUavIndex_);
    srvManager_->SetComputeRootDescriptorTable(ScatterCompactRP::kDrawIndexUavParam, drawIndexUavIndex_);
    srvManager_->SetComputeRootDescriptorTable(ScatterCompactRP::kScatterCursorUavParam, scatterCursorUavIndex_);
    {
      uint32_t scatterGroups = (kNumMaxParticle + 255) / 256;
      commandList->Dispatch(scatterGroups, 1, 1);
    }
    dx12_->SetUAVBarrier(drawIndexResource_.Get());

    /// ======================== ///
    ///           描画    　     ///
    /// ======================= ///

    DrawParticleGraphics(perViewResource_->GetGPUVirtualAddress(), -1);
  }

  void GPUParticle::DrawParticleGraphics(D3D12_GPU_VIRTUAL_ADDRESS perViewAddress, int32_t slotFilter)
  {
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

    // ルートシグネチャの設定
    commandList->SetGraphicsRootSignature(RS_.Get());

    // プリミティブトポロジを設定
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // per-instance パーティクル index ストリーム (slot1) のみバインド。
    // 頂点/インデックスは VS が SRV(t3/t4) からプルするため slot0 頂点バッファ・IBV は不要。
    commandList->IASetVertexBuffers(1, 1, &drawIndexVBV_);

    // ParticleData は VS で SRV(t0) として読むため NON_PIXEL へ遷移
    dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, particleResource_.Get());
    // drawIndexList は per-instance VBV として使うため VERTEX_AND_CONSTANT_BUFFER へ遷移
    dx12_->TransitionResourceWithTracking(drawIndexResource_.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    // DrawArgs を ExecuteIndirect の引数 state へ遷移
    dx12_->TransitionResourceWithTracking(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // 全エミッター共通のルート: ParticleData SRV(t0), PerView CBV(b0), Emitter SRV(t2: ビルボード判定用)
    srvManager_->SetGraphicsRootDescriptorTable(DrawRP::kParticleSrvParam, particleSrvIndex_);
    commandList->SetGraphicsRootConstantBufferView(DrawRP::kPerViewCbvParam, perViewAddress);
    srvManager_->SetGraphicsRootDescriptorTable(DrawRP::kEmitterSrvParam, emitterSrvIndex_);

    // per-emitter ループ: エミッターごとに PSO(ブレンドモード)/テクスチャ/
    // 描画モデル(頂点 SRV t3・index SRV t4) を切り替えて 1 つずつ ExecuteIndirect。
    // instanceCount / StartInstanceLocation(=base_e) / VertexCountPerInstance(=描画モデル index 数) は
    // GPU 側 (BuildDrawArgs) 計算済み。描画モデル未指定のエミッターはデフォルト板ポリ SRV にフォールバックする。
    const size_t emitterCount = std::min(activeEmitters_.size(), static_cast<size_t>(kNumMaxEmitter));
    const UINT drawStride = static_cast<UINT>(sizeof(D3D12_DRAW_ARGUMENTS));
    for (size_t i = 0; i < emitterCount; ++i) {
      if (slotFilter >= 0 && static_cast<int32_t>(i) != slotFilter) continue;
      const auto& emitter = activeEmitters_[i];
      if (!emitter) continue;
      const EmitterData& ed = emitter->GetData();
      const uint32_t texIndex = (ed.textureSrvIndex != 0) ? ed.textureSrvIndex : modelData_.textureData.textureIndex;
      const uint32_t vtxSrv = (ed.renderVertexSrvIndex != 0) ? ed.renderVertexSrvIndex : defaultQuadVertexSrvIndex_;
      const uint32_t idxSrv = (ed.renderIndexSrvIndex != 0) ? ed.renderIndexSrvIndex : defaultQuadIndexSrvIndex_;

      commandList->SetPipelineState(GetBlendPSO(ed.blendMode));      // ブレンドモード
      srvManager_->SetGraphicsRootDescriptorTable(DrawRP::kTextureParam, texIndex);        // テクスチャ (t0, PS)
      srvManager_->SetGraphicsRootDescriptorTable(DrawRP::kModelVertexParam, vtxSrv);      // 描画モデル頂点 (t3)
      srvManager_->SetGraphicsRootDescriptorTable(DrawRP::kModelIndexParam, idxSrv);       // 描画モデルインデックス (t4)
      commandList->ExecuteIndirect(drawCommandSignature_.Get(), 1, drawArgsResource_.Get(),
        static_cast<UINT64>(i) * drawStride, nullptr, 0);
    }

    // 各リソースの state を UAV に戻す
    dx12_->TransitionResourceWithTracking(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dx12_->TransitionResourceWithTracking(drawIndexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, particleResource_.Get());
  }

#ifdef _DEBUG
  void GPUParticle::DrawEmitterForPreview(int32_t slot, Camera* previewCamera)
  {
    if (!previewCamera || !previewPerViewData_) {
      return;
    }

    // UpdatePerView と同式（デバッグカメラ分岐なし）をプレビューカメラで計算
    const Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f, .y = 1.0f, .z = 1.0f }, previewCamera->GetRotate(), previewCamera->GetTranslate());
    previewPerViewData_->viewProjection = Mat4x4::Multiply(Mat4x4::Inverse(cameraMatrix), previewCamera->GetProjectionMatrix());

    Matrix4x4 billboardMatrix = Mat4x4::Multiply(Mat4x4::MakeRotateY(std::numbers::pi_v<float>), cameraMatrix);
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;
    previewPerViewData_->billboardMatrix = billboardMatrix;

    DrawParticleGraphics(previewPerViewResource_->GetGPUVirtualAddress(), slot);
  }
#endif

  void GPUParticle::Finalize()
  {
    // 全 SRV/UAV インデックスを返却
    SrvManager* srvManager = SrvManager::GetInstance();
    srvManager->Free(particleUavIndex_);
    srvManager->Free(particleSrvIndex_);
    srvManager->Free(emitterSrvIndex_);
    srvManager->Free(freeListIndexUavIndex_);
    srvManager->Free(freeListUavIndex_);
    srvManager->Free(forceFieldSrvIndex_);
    srvManager->Free(depthSrvIndex_);
    srvManager->Free(defaultQuadVertexSrvIndex_);
    srvManager->Free(defaultQuadIndexSrvIndex_);
    srvManager->Free(perEmitterCountUavIndex_);
    srvManager->Free(drawIndexUavIndex_);
    srvManager->Free(drawIndexSrvIndex_);
    srvManager->Free(scatterCursorUavIndex_);
    srvManager->Free(drawArgsUavIndex_);
    srvManager->Free(emitterIndexCountSrvIndex_);

    // クローン保持している Model の SRV を返却（shared_ptr 破棄では Finalize が呼ばれないため明示的に呼ぶ）
    for (auto& [path, model] : renderModels_) {
      if (model) {
        model->Finalize();
      }
    }

    instance_.reset();
  }

  void GPUParticle::OnResize()
  {
    // 深度バッファが再作成されるため SRV を再構築
    CreateDepthSRV();
  }

  Model* GPUParticle::AcquireModel(const std::string& modelPath)
  {
    // 同一 path はキャッシュを返す。未ロードならロードし、Model クローンをシステムが保持する。
    auto it = renderModels_.find(modelPath);
    if (it == renderModels_.end()) {
      ModelManager::GetInstance()->LoadModel(modelPath);
      std::shared_ptr<Model> model = ModelManager::GetInstance()->GetModel(modelPath); // unique_ptr → shared_ptr へ移譲
      if (!model || model->GetMeshCount() == 0) return nullptr;
      it = renderModels_.emplace(modelPath, std::move(model)).first;
    }
    return it->second.get();
  }

  Mesh* GPUParticle::AcquireModelMesh(const std::string& modelPath)
  {
    Model* model = AcquireModel(modelPath);
    return model ? model->GetMesh(0) : nullptr;
  }

  std::shared_ptr<GPUParticleEmitter> GPUParticle::CreateTemporaryEmitterFrom(GPUParticleEmitter* sourceEmitter, float lifeTime)
  {
    if (!sourceEmitter) return nullptr;

    auto newEmitter = sourceEmitter->Clone();
    if (!newEmitter) return nullptr;

    newEmitter->SetActive(true);

    // frequencyTime を frequency に合わせ次フレームで即射出
    newEmitter->SetFrequencyTime(newEmitter->GetFrequency());

    newEmitter->SetEmitting(true);

    newEmitter->SetTemporary(true, lifeTime);

    RegisterEmitter(newEmitter);

#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("CreateTempEmitter: ID=" + std::to_string(newEmitter->GetEmitterId()) +
      ", Active=" + std::to_string(newEmitter->IsActive() ? 1 : 0) +
      ", Emit=" + std::to_string(newEmitter->IsEmitting() ? 1 : 0) +
      ", FreqTime=" + std::to_string(newEmitter->GetFrequencyTime()) +
      "/" + std::to_string(newEmitter->GetFrequency()), DebugUIManager::LogType::Info);
#endif

    return newEmitter;
  }

  void GPUParticle::RegisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter)
  {
    if (!emitter) {
      return;
    }

    //  空きスロットがあれば再利用し、無ければ末尾に新規確保する。
    //  スロット番号 = 正式な emitterID (ctor の 0 は仮値)。粒子の emitterId・drawArgs のインデックスと一致する。
    if (!freeEmitterSlots_.empty()) {
      const uint32_t slot = freeEmitterSlots_.back();
      freeEmitterSlots_.pop_back();
      emitter->data_.emitterID = slot;
      activeEmitters_[slot] = emitter;
    }
    else {
      if (activeEmitters_.size() >= kNumMaxEmitter) {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "RegisterEmitter: emitter slots exhausted (max " + std::to_string(kNumMaxEmitter) + ")",
          DebugUIManager::LogType::Error);
#endif
        return;
      }
      emitter->data_.emitterID = static_cast<uint32_t>(activeEmitters_.size());
      activeEmitters_.push_back(emitter);
    }
  }

  void GPUParticle::UnregisterEmitter(std::shared_ptr<GPUParticleEmitter> emitter)
  {
    if (!emitter || activeEmitters_.empty()) {
      return;
    }

    auto it = std::ranges::find(activeEmitters_, emitter);
    if (it == activeEmitters_.end()) {
      return;
    }

    //スロットは保持したまま射出だけ停止し、既存パーティクルは寿命まで正しいエミッター(描画モデル/ブレンド/テクスチャ)で描き切る。パーティクルが全滅する頃
    // に RetireExpiredSlots がスロットを解放し、freeEmitterSlots_ へ返す。
    const uint32_t slot = static_cast<uint32_t>(it - activeEmitters_.begin());

    // 既に退役登録済みのスロットは二重登録しない (二重解放→スロット重複割り当てを防ぐ)。
    for (const auto& r : retiringSlots_) {
      if (r.first == slot) {
        return;
      }
    }

    emitter->SetActive(false);
    emitter->SetEmitting(false);

    // 解放時刻 = 現在時刻 + 粒子寿命の上限 + マージン。。
    constexpr float kEmitterRetireMargin = 0.25f;   // フレーム境界の取りこぼし防止マージン
    constexpr float kDefaultParticleMaxLife = 1.5f; // EmitParticle.CS.hlsl:518 の非ランダム化時デフォルト寿命上限と一致させること
    const float now = FrameTimer::GetInstance()->GetGameTime();
    const float maxLifeTime = (std::max)(emitter->GetLifeTimeRange().y, kDefaultParticleMaxLife);
    const float freeAt = now + maxLifeTime + kEmitterRetireMargin;
    retiringSlots_.emplace_back(slot, freeAt);
  }

  //--------------------------------------Private--------------------------------------//

  void GPUParticle::RetireExpiredSlots()
  {
    if (retiringSlots_.empty()) {
      return;
    }
    // 退役後、最大寿命が経過したスロットを解放する。
    const float now = FrameTimer::GetInstance()->GetGameTime();
    std::erase_if(retiringSlots_, [&](const std::pair<uint32_t, float>& r) {
      if (now < r.second) {
        return false;
      }
      activeEmitters_[r.first] = nullptr;    // スロットを空に
      freeEmitterSlots_.push_back(r.first);  // 再利用可能リストへ返却
      return true;
      });
  }

  void GPUParticle::UpdateEmitter()
  {
    float deltaTime = FrameTimer::GetInstance()->GetDeltaTime();

    // 退役済みスロットのうち寿命切れのものを解放する。
    RetireExpiredSlots();

    // すべてのアクティブなエミッターの射出タイマーを更新する。
    for (auto& emitter : activeEmitters_) {
      // 空きスロットまたは非アクティブなら射出しない
      if (!emitter || !emitter->IsActive()) {
        continue;
      }

      // 射出タイマーを更新
      emitter->UpdateEmission(deltaTime);
    }

    // GPU 側のデータを同期
    SyncEmitterData();
  }

  void GPUParticle::UpdatePerView()
  {

    Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, camera_->GetRotate(), camera_->GetTranslate());

#ifdef _DEBUG
    if (isDebug_) {
      cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, DebugCamera::GetInstance()->GetRotate(), DebugCamera::GetInstance()->GetTranslate());
    }
#endif

    const Matrix4x4 viewProjectionMatrix = Mat4x4::Multiply(Mat4x4::Inverse(cameraMatrix), camera_->GetProjectionMatrix());

    // ビルボード行列の生成
    const Matrix4x4 backToFrontMatrix = Mat4x4::MakeRotateY(std::numbers::pi_v<float>);

    Matrix4x4 billboardMatrix = Mat4x4::Multiply(backToFrontMatrix, cameraMatrix);
    billboardMatrix.m[3][0] = 0.0f;  //平行移動成分はいらない
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    // PerView の更新
    perViewData_->billboardMatrix = billboardMatrix;
    perViewData_->viewProjection = viewProjectionMatrix;
  }

  void GPUParticle::UpdatePerFrame()
  {
    perFrameData_->time = FrameTimer::GetInstance()->GetGameTime();
    perFrameData_->deltaTime = FrameTimer::GetInstance()->GetDeltaTime();
    perFrameData_->frameCount++;
  }

  // CPU→GPU 同期
  void GPUParticle::SyncEmitterData()
  {
    // GPU 側のエミッターバッファにマップ
    EmitterData* gpuEmitters = nullptr;
    emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));

    // バッファサイズが足りるか確認
    if (activeEmitters_.size() > kNumMaxEmitter) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog("Warning: Too many active emitters! Max: " + std::to_string(kNumMaxEmitter) +
        ", Current: " + std::to_string(activeEmitters_.size()), DebugUIManager::LogType::Warning);
#endif
    }

    // 各エミッターの GPU データを更新
    size_t emitterCount = std::min(activeEmitters_.size(), static_cast<size_t>(kNumMaxEmitter));
    for (size_t i = 0; i < emitterCount; i++) {
      if (activeEmitters_[i]) {
        const EmitterData& ed = activeEmitters_[i]->GetData();
        gpuEmitters[i] = ed;
        // 描画テンプレート: 描画モデルの index 数 (= 1パーティクルあたりの描画頂点数)。
        // 描画モデル未指定 (renderIndexCount==0) のエミッターはデフォルト板ポリ。
        if (emitterIndexCountData_) {
          emitterIndexCountData_[i] = (ed.renderIndexCount != 0u) ? ed.renderIndexCount : defaultQuadIndexCount_;
        }
      }
      else {
        // 退役後に解放された空きスロット: EmitParticle/描画でスキップさせるため無効化する。
        // 前占有エミッターの古い flags が残ると誤射出するので明示的にゼロにする。
        gpuEmitters[i].flags = 0u;
        gpuEmitters[i].renderIndexCount = 0u;
        if (emitterIndexCountData_) {
          emitterIndexCountData_[i] = defaultQuadIndexCount_;
        }
      }
    }

    // アンマップ
    emitterResource_->Unmap(0, nullptr);

    // PerFrame データにアクティブエミッター数を格納
    perFrameData_->activeEmitterCount = static_cast<uint32_t>(emitterCount);
  }

  void GPUParticle::CreateRS()
  {
    HRESULT hr;

    // 全パーティクル共通の描画ルートシグネチャ。
    // VS(GPUParticle.VS) が頂点/インデックスを SRV プルするため、描画モデル(既定板ポリ含む)の
    // 頂点 SRV(t3)・インデックス SRV(t4) をルートに持つ。
    // ([0]=Particle SRV t0(VS) / [1]=PerView CBV b0(VS) / [2]=Texture SRV t0(PS) /
    //  [3]=Emitter SRV t2(VS) / [4]=描画モデル頂点 SRV t3(VS) / [5]=描画モデルインデックス SRV t4(VS))
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Static Sampler (PS テクスチャサンプリング用)
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    // SRV レンジ生成ヘルパ (1 ディスクリプタ, append offset)
    auto makeSrvRange = [](UINT reg) {
      D3D12_DESCRIPTOR_RANGE r{};
      r.BaseShaderRegister = reg;
      r.NumDescriptors = 1;
      r.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
      return r;
      };
    D3D12_DESCRIPTOR_RANGE rangeParticle[1]  = { makeSrvRange(0) }; // t0
    D3D12_DESCRIPTOR_RANGE rangeTex[1]       = { makeSrvRange(0) }; // t0 (PS)
    D3D12_DESCRIPTOR_RANGE rangeEmitter[1]   = { makeSrvRange(2) }; // t2
    D3D12_DESCRIPTOR_RANGE rangeRenderVtx[1] = { makeSrvRange(3) }; // t3
    D3D12_DESCRIPTOR_RANGE rangeRenderIdx[1] = { makeSrvRange(4) }; // t4

    D3D12_ROOT_PARAMETER rootParameters[6] = {};
    // [0] Particle SRV (t0, VS)
    rootParameters[DrawRP::kParticleSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[DrawRP::kParticleSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[DrawRP::kParticleSrvParam].DescriptorTable.pDescriptorRanges = rangeParticle;
    rootParameters[DrawRP::kParticleSrvParam].DescriptorTable.NumDescriptorRanges = 1;
    // [1] PerView CBV (b0, VS)
    rootParameters[DrawRP::kPerViewCbvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[DrawRP::kPerViewCbvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[DrawRP::kPerViewCbvParam].Descriptor.ShaderRegister = 0;
    // [2] Texture SRV (t0, PS)
    rootParameters[DrawRP::kTextureParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[DrawRP::kTextureParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[DrawRP::kTextureParam].DescriptorTable.pDescriptorRanges = rangeTex;
    rootParameters[DrawRP::kTextureParam].DescriptorTable.NumDescriptorRanges = 1;
    // [3] Emitter SRV (t2, VS) — ビルボードフラグ参照
    rootParameters[DrawRP::kEmitterSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[DrawRP::kEmitterSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[DrawRP::kEmitterSrvParam].DescriptorTable.pDescriptorRanges = rangeEmitter;
    rootParameters[DrawRP::kEmitterSrvParam].DescriptorTable.NumDescriptorRanges = 1;
    // [4] 描画モデル頂点 SRV (t3, VS)
    rootParameters[DrawRP::kModelVertexParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[DrawRP::kModelVertexParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[DrawRP::kModelVertexParam].DescriptorTable.pDescriptorRanges = rangeRenderVtx;
    rootParameters[DrawRP::kModelVertexParam].DescriptorTable.NumDescriptorRanges = 1;
    // [5] 描画モデルインデックス SRV (t4, VS)
    rootParameters[DrawRP::kModelIndexParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[DrawRP::kModelIndexParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[DrawRP::kModelIndexParam].DescriptorTable.pDescriptorRanges = rangeRenderIdx;
    rootParameters[DrawRP::kModelIndexParam].DescriptorTable.NumDescriptorRanges = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      if (errorBlob) DebugUIManager::GetInstance()->AddLog(static_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(RS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateDrawPSO(const D3D12_BLEND_DESC& blendDesc, Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPSO)
  {
    HRESULT hr;

    // InputLayout: per-instance パーティクル index のみ (slot1, R32_UINT)。
    // 頂点/インデックスは VS(GPUParticle.VS) が SRV(t3/t4) からプルするため slot0 頂点バッファは無し。
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "TEXCOORD";
    inputElementDescs[0].SemanticIndex = 1;
    inputElementDescs[0].Format = DXGI_FORMAT_R32_UINT;
    inputElementDescs[0].InputSlot = 1;
    inputElementDescs[0].AlignedByteOffset = 0;
    inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
    inputElementDescs[0].InstanceDataStepRate = 1;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // RasterizerState
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

    // shader のコンパイル 
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"GPUParticle.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"GPUParticle.PS.hlsl"), L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState (透明描画なので深度書き込みなし)
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = RS_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&outPSO));
    assert(SUCCEEDED(hr));
  }

  ID3D12PipelineState* GPUParticle::GetBlendPSO(uint32_t blendMode) const
  {
    switch (static_cast<ParticleBlendMode>(blendMode)) {
    case ParticleBlendMode::Add:   return psoAdd_.Get();
    case ParticleBlendMode::Alpha: return psoAlpha_.Get();
    case ParticleBlendMode::Screen:
    default:                       return psoScreen_.Get();
    }
  }

  void GPUParticle::CreateInitComputeRS()
  {
    HRESULT hr;
    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {}; // Particle
    descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeListIndex[1] = {}; // FreeListIndex
    descriptorRangeForFreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRangeForFreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForFreeList[1] = {}; // FreeList
    descriptorRangeForFreeList[0].BaseShaderRegister = 2; // レジスタ番号
    descriptorRangeForFreeList[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // Particle
    rootParameters[InitCsRP::kParticleUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[InitCsRP::kParticleUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[InitCsRP::kParticleUavParam].DescriptorTable.pDescriptorRanges = descriptorRangeForParticle; // ディスクリプタレンジを設定
    rootParameters[InitCsRP::kParticleUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForParticle); // レンジの数

    // FreeListIndex
    rootParameters[InitCsRP::kFreeListIndexUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[InitCsRP::kFreeListIndexUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[InitCsRP::kFreeListIndexUavParam].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeListIndex; // ディスクリプタレンジを設定
    rootParameters[InitCsRP::kFreeListIndexUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeListIndex); // レンジの数

    // FreeList
    rootParameters[InitCsRP::kFreeListUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[InitCsRP::kFreeListUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[InitCsRP::kFreeListUavParam].DescriptorTable.pDescriptorRanges = descriptorRangeForFreeList; // ディスクリプタレンジを設定
    rootParameters[InitCsRP::kFreeListUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForFreeList); // レンジの数

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(initComputeRS_.GetAddressOf()));
  }

  void GPUParticle::CreateEmitParticleComputeRS()
  {
    HRESULT hr;
    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRange_Particle[1] = {}; // Particle
    descriptorRange_Particle[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRange_Particle[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_Particle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_Particle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_FreeListIndex[1] = {}; // FreeListIndex
    descriptorRange_FreeListIndex[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRange_FreeListIndex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_FreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_FreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_FreeList[1] = {}; // FreeList
    descriptorRange_FreeList[0].BaseShaderRegister = 2; // レジスタ番号
    descriptorRange_FreeList[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_FreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRange_FreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRange_Emitter[1] = {}; // Emitter
    descriptorRange_Emitter[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRange_Emitter[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRange_Emitter[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRange_Emitter[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // Mesh エミッタ用の頂点/インデックス SRV (t10, t11 の固定スロット)
    D3D12_DESCRIPTOR_RANGE descriptorRange_MeshVertices[1] = {};
    descriptorRange_MeshVertices[0].BaseShaderRegister = 10; // t10
    descriptorRange_MeshVertices[0].NumDescriptors = 1;
    descriptorRange_MeshVertices[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_MeshVertices[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRange_MeshIndices[1] = {};
    descriptorRange_MeshIndices[0].BaseShaderRegister = 11; // t11
    descriptorRange_MeshIndices[0].NumDescriptors = 1;
    descriptorRange_MeshIndices[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_MeshIndices[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Mesh Area Prefix Sum SRV (t12)
    D3D12_DESCRIPTOR_RANGE descriptorRange_MeshAreaPrefixSum[1] = {};
    descriptorRange_MeshAreaPrefixSum[0].BaseShaderRegister = 12; // t12
    descriptorRange_MeshAreaPrefixSum[0].NumDescriptors = 1;
    descriptorRange_MeshAreaPrefixSum[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_MeshAreaPrefixSum[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameter: Particle/FreeListIndex/FreeList UAV + Emitter/MeshVtx/MeshIdx/MeshAreaPrefixSum SRV + PerFrame CBV + RootConstants = 9
    D3D12_ROOT_PARAMETER rootParameters[9] = {};
    // Particle
    rootParameters[EmitCsRP::kParticleUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[EmitCsRP::kParticleUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[EmitCsRP::kParticleUavParam].DescriptorTable.pDescriptorRanges = descriptorRange_Particle; // ディスクリプタレンジを設定
    rootParameters[EmitCsRP::kParticleUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_Particle); // レンジの数

    // EmitterSphere
    rootParameters[EmitCsRP::kEmitterSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[EmitCsRP::kEmitterSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[EmitCsRP::kEmitterSrvParam].DescriptorTable.pDescriptorRanges = descriptorRange_Emitter; // ディスクリプタレンジを設定
    rootParameters[EmitCsRP::kEmitterSrvParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_Emitter); // レンジの数

    // PerFrame
    rootParameters[EmitCsRP::kPerFrameCbvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[EmitCsRP::kPerFrameCbvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[EmitCsRP::kPerFrameCbvParam].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    // FreeListIndex
    rootParameters[EmitCsRP::kFreeListIndexUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[EmitCsRP::kFreeListIndexUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[EmitCsRP::kFreeListIndexUavParam].DescriptorTable.pDescriptorRanges = descriptorRange_FreeListIndex; // ディスクリプタレンジを設定
    rootParameters[EmitCsRP::kFreeListIndexUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_FreeListIndex); // レンジの数

    // FreeList
    rootParameters[EmitCsRP::kFreeListUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[EmitCsRP::kFreeListUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[EmitCsRP::kFreeListUavParam].DescriptorTable.pDescriptorRanges = descriptorRange_FreeList; // ディスクリプタレンジを設定
    rootParameters[EmitCsRP::kFreeListUavParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_FreeList); // レンジの数

    // Mesh Vertex SRV (t10) - Mesh エミッタの頂点バッファ
    rootParameters[EmitCsRP::kMeshVertexParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[EmitCsRP::kMeshVertexParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[EmitCsRP::kMeshVertexParam].DescriptorTable.pDescriptorRanges = descriptorRange_MeshVertices;
    rootParameters[EmitCsRP::kMeshVertexParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_MeshVertices);

    // Mesh Index SRV (t11) - Mesh エミッタのインデックスバッファ
    rootParameters[EmitCsRP::kMeshIndexParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[EmitCsRP::kMeshIndexParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[EmitCsRP::kMeshIndexParam].DescriptorTable.pDescriptorRanges = descriptorRange_MeshIndices;
    rootParameters[EmitCsRP::kMeshIndexParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_MeshIndices);

    // Mesh Area Prefix Sum SRV (t12)
    rootParameters[EmitCsRP::kMeshAreaPrefixSumParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[EmitCsRP::kMeshAreaPrefixSumParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[EmitCsRP::kMeshAreaPrefixSumParam].DescriptorTable.pDescriptorRanges = descriptorRange_MeshAreaPrefixSum;
    rootParameters[EmitCsRP::kMeshAreaPrefixSumParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_MeshAreaPrefixSum);

    // RootConstants (b1) - gTargetMeshEmitterId
    rootParameters[EmitCsRP::kTargetMeshEmitterIdParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[EmitCsRP::kTargetMeshEmitterIdParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[EmitCsRP::kTargetMeshEmitterIdParam].Constants.ShaderRegister = 1; // b1
    rootParameters[EmitCsRP::kTargetMeshEmitterIdParam].Constants.RegisterSpace = 0;
    rootParameters[EmitCsRP::kTargetMeshEmitterIdParam].Constants.Num32BitValues = 1; // uint gTargetMeshEmitterId

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(static_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(emitParticleRS_.GetAddressOf()));
  }

  void GPUParticle::CreateComputeShaderPSO(Microsoft::WRL::ComPtr<ID3D12RootSignature>& RS, Microsoft::WRL::ComPtr<ID3D12PipelineState>& PSO, const std::wstring& shaderName)
  {
    Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dx12_->CompileShader(EnginePaths::ShaderPath(shaderName), L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
    computePipelineStateDesc.pRootSignature = RS.Get();
    computePipelineStateDesc.CS = { .pShaderBytecode = csBlob->GetBufferPointer(), .BytecodeLength = csBlob->GetBufferSize() };

    HRESULT hr = dx12_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&PSO));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateDefaultQuadMesh()
  {
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = 1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 0.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 0.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });
    modelData_.vertices.push_back({ .position = {.x = -1.0f, .y = -1.0f, .z = 0.0f, .w = 1.0f}, .texcoord = {.x = 1.0f, .y = 1.0f}, .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f} });

    // 頂点 StructuredBuffer + SRV (VS が SV_VertexID でプルする)
    const UINT vertexCount = static_cast<UINT>(modelData_.vertices.size());
    defaultQuadVertexResource_ = dx12_->MakeBufferResource(sizeof(VertexData) * vertexCount);
    VertexData* mappedVtx = nullptr;
    defaultQuadVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVtx));
    std::memcpy(mappedVtx, modelData_.vertices.data(), sizeof(VertexData) * vertexCount);
    defaultQuadVertexResource_->Unmap(0, nullptr);
    defaultQuadVertexSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(defaultQuadVertexSrvIndex_, defaultQuadVertexResource_.Get(), vertexCount, sizeof(VertexData));

    // インデックス StructuredBuffer + SRV ({0,1,2,3,4,5} 素通し。2 三角形)
    const uint32_t indices[6] = { 0, 1, 2, 3, 4, 5 };
    defaultQuadIndexCount_ = _countof(indices);
    defaultQuadIndexResource_ = dx12_->MakeBufferResource(sizeof(indices));
    uint32_t* mappedIdx = nullptr;
    defaultQuadIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIdx));
    std::memcpy(mappedIdx, indices, sizeof(indices));
    defaultQuadIndexResource_->Unmap(0, nullptr);
    defaultQuadIndexSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(defaultQuadIndexSrvIndex_, defaultQuadIndexResource_.Get(), defaultQuadIndexCount_, sizeof(uint32_t));
  }

  void GPUParticle::CreatePerViewData()
  {
    dx12_->CreateBufferResource(perViewResource_, sizeof(PerView));

    // map
    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

    // データの設定
    perViewData_->viewProjection = Mat4x4::MakeIdentity();
    perViewData_->billboardMatrix = Mat4x4::MakeIdentity();

#ifdef _DEBUG
    // エディタプレビュー用の第2 PerView（本編と同一フレームで別視点を併存させるため分離）
    dx12_->CreateBufferResource(previewPerViewResource_, sizeof(PerView));
    previewPerViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&previewPerViewData_));
    previewPerViewData_->viewProjection = Mat4x4::MakeIdentity();
    previewPerViewData_->billboardMatrix = Mat4x4::MakeIdentity();
#endif
  }

  void GPUParticle::CreatePerFrameData()
  {
    // PerFrame のリソースを生成
    dx12_->CreateBufferResource(perFrameResource_, sizeof(PerFrame));
    // PerFrame のデータをマップ
    perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));
    // PerFrame のデータを初期化
    perFrameData_->time = 0.0f;
    perFrameData_->deltaTime = 0.0f;
    // 毎起動異なる乱数ストリームを保証するため random_device で抽選
    perFrameData_->frameCount = std::random_device{}();
  }

  void GPUParticle::CreateEmitterData()
  {

    // エミッターリソースの生成
    dx12_->CreateBufferResource(emitterResource_, sizeof(EmitterData) * kNumMaxEmitter);

    // エミッターリソースの SRV を作成
    emitterSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(emitterSrvIndex_, emitterResource_.Get(), kNumMaxEmitter, sizeof(EmitterData));

    // エミッター配列とスロット管理状態の初期化
    activeEmitters_.clear();
    freeEmitterSlots_.clear();
    retiringSlots_.clear();

    // GPU 側の初期化
    EmitterData* gpuEmitters = nullptr;
    emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuEmitters));
    ZeroMemory(gpuEmitters, sizeof(EmitterData) * kNumMaxEmitter);
    emitterResource_->Unmap(0, nullptr);
  }

  void GPUParticle::CreateParticleResource()
  {
    // ParticleCS のリソースを生成
    dx12_->CreateResourceForUAV(particleResource_, sizeof(ParticleCS) * kNumMaxParticle);

    // ParticleCS の UAV を生成
    particleUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(particleUavIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));

    // ParticleCS の SRV を生成
    particleSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kNumMaxParticle, sizeof(ParticleCS));
  }

  void GPUParticle::CreateFreeListResource()
  {
    // FreeListIndex のリソースを生成
    dx12_->CreateResourceForUAV(freeListIndexResource_, sizeof(int32_t));

    // FreeListIndex の UAV を生成
    freeListIndexUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(freeListIndexUavIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));


    // FreeList のリソースを生成
    dx12_->CreateResourceForUAV(freeListResource_, sizeof(uint32_t) * kNumMaxParticle);

    // FreeList の UAV を生成
    freeListUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(freeListUavIndex_, freeListResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
  }

  void GPUParticle::CreateIndirectResources()
  {
    // --- per-emitter 生存数カウンタ (uint kNumMaxEmitter 要素) ---
    dx12_->CreateResourceForUAV(perEmitterCountResource_, sizeof(uint32_t) * kNumMaxEmitter);
    dx12_->SetInitialResourceState(perEmitterCountResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    perEmitterCountUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(perEmitterCountUavIndex_, perEmitterCountResource_.Get(), kNumMaxEmitter, sizeof(uint32_t));

    // --- コンパクション済み生存 index 配列 (uint kNumMaxParticle 要素) ---
    dx12_->CreateResourceForUAV(drawIndexResource_, sizeof(uint32_t) * kNumMaxParticle);
    dx12_->SetInitialResourceState(drawIndexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    drawIndexUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(drawIndexUavIndex_, drawIndexResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
    drawIndexSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(drawIndexSrvIndex_, drawIndexResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
    // per-instance 頂点ストリームとしてのビュー (R32_UINT, stride 4)。
    // 各エミッターの描画は ExecuteIndirect の StartInstanceLocation=base_e でこのストリームをオフセット参照する。
    drawIndexVBV_.BufferLocation = drawIndexResource_->GetGPUVirtualAddress();
    drawIndexVBV_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * kNumMaxParticle);
    drawIndexVBV_.StrideInBytes = sizeof(uint32_t);

    // --- per-emitter スキャッタ用カーソル (uint kNumMaxEmitter 要素、各 base_e で初期化される) ---
    dx12_->CreateResourceForUAV(scatterCursorResource_, sizeof(uint32_t) * kNumMaxEmitter);
    dx12_->SetInitialResourceState(scatterCursorResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    scatterCursorUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(scatterCursorUavIndex_, scatterCursorResource_.Get(), kNumMaxEmitter, sizeof(uint32_t));

    // --- per-emitter Indirect 描画引数 (D3D12_DRAW_ARGUMENTS kNumMaxEmitter 要素) ---
    dx12_->CreateResourceForUAV(drawArgsResource_, sizeof(D3D12_DRAW_ARGUMENTS) * kNumMaxEmitter);
    dx12_->SetInitialResourceState(drawArgsResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    drawArgsUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(drawArgsUavIndex_, drawArgsResource_.Get(), kNumMaxEmitter, sizeof(D3D12_DRAW_ARGUMENTS));

    // --- per-emitter 描画テンプレート (UPLOAD: 描画モデルの index 数。既定板ポリは 6) ---
    dx12_->CreateBufferResource(emitterIndexCountResource_, sizeof(uint32_t) * kNumMaxEmitter);
    emitterIndexCountSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(emitterIndexCountSrvIndex_, emitterIndexCountResource_.Get(), kNumMaxEmitter, sizeof(uint32_t));
    emitterIndexCountResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterIndexCountData_));
    for (uint32_t i = 0; i < kNumMaxEmitter; ++i) emitterIndexCountData_[i] = defaultQuadIndexCount_; // 既定: 全 quad (6)
  }

  void GPUParticle::CreateCommandSignature()
  {
    // DRAW (非indexed) 単体のコマンドシグネチャ。全パーティクル描画 (既定板ポリ / カスタムモデル) で共用。
    // 頂点/インデックスは VS が SRV プルし、オフセットは per-instance VBV の StartInstanceLocation で
    // 与えるため、ルート引数を含まない。
    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
    sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS); // 16
    sigDesc.NumArgumentDescs = 1;
    sigDesc.pArgumentDescs = &argDesc;
    sigDesc.NodeMask = 0;

    HRESULT hr = dx12_->GetDevice()->CreateCommandSignature(
      &sigDesc, nullptr, IID_PPV_ARGS(&drawCommandSignature_));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateResetCountersRS()
  {
    // u0: perEmitterCount UAV
    D3D12_DESCRIPTOR_RANGE range[1] = {};
    range[0].BaseShaderRegister = 0;
    range[0].NumDescriptors = 1;
    range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[ResetCountersRP::kPerEmitterCountUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[ResetCountersRP::kPerEmitterCountUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[ResetCountersRP::kPerEmitterCountUavParam].DescriptorTable.pDescriptorRanges = range;
    rootParameters[ResetCountersRP::kPerEmitterCountUavParam].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
#ifdef _DEBUG
      if (err) DebugUIManager::GetInstance()->AddLog(static_cast<char*>(err->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(resetCountersRS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateBuildDrawArgsRS()
  {
    // u0: perEmitterCount, u1: DrawArgs(DRAW), u2: ScatterCursor (UAV)
    // t0: emitterTemplate (SRV)
    D3D12_DESCRIPTOR_RANGE uavRanges[3] = {};
    for (uint32_t i = 0; i < 3; ++i) {
      uavRanges[i].BaseShaderRegister = i;
      uavRanges[i].NumDescriptors = 1;
      uavRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      uavRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
    srvRange[0].BaseShaderRegister = 0; // t0
    srvRange[0].NumDescriptors = 1;
    srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    // スロット 0..2 = BuildDrawArgsRP の kPerEmitterCountUavParam / kDrawArgsUavParam / kScatterCursorUavParam (u0..u2)
    for (uint32_t i = 0; i < 3; ++i) {
      rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
      rootParameters[i].DescriptorTable.pDescriptorRanges = &uavRanges[i];
      rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
    }
    rootParameters[BuildDrawArgsRP::kEmitterIndexCountSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[BuildDrawArgsRP::kEmitterIndexCountSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[BuildDrawArgsRP::kEmitterIndexCountSrvParam].DescriptorTable.pDescriptorRanges = srvRange;
    rootParameters[BuildDrawArgsRP::kEmitterIndexCountSrvParam].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
#ifdef _DEBUG
      if (err) DebugUIManager::GetInstance()->AddLog(static_cast<char*>(err->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(buildDrawArgsRS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateScatterCompactRS()
  {
    // u0: Particles, u1: DrawIndexList, u2: ScatterCursor (すべて UAV)
    D3D12_DESCRIPTOR_RANGE ranges[3] = {};
    for (uint32_t i = 0; i < 3; ++i) {
      ranges[i].BaseShaderRegister = i;
      ranges[i].NumDescriptors = 1;
      ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // スロット 0..2 = ScatterCompactRP の kParticleUavParam / kDrawIndexUavParam / kScatterCursorUavParam (u0..u2)
    for (uint32_t i = 0; i < 3; ++i) {
      rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
      rootParameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
      rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
#ifdef _DEBUG
      if (err) DebugUIManager::GetInstance()->AddLog(static_cast<char*>(err->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(scatterCompactRS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

#ifdef _DEBUG
  void GPUParticle::CreateFreeListReadbackResource()
  {
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(int32_t);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    HRESULT hr = dx12_->GetDevice()->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&freeListIndexReadbackResource_));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::ReadbackActiveParticleCount()
  {
    // 間引き制御: kReadbackInterval フレームに1回だけ実行
    readbackFrameCounter_++;
    if (readbackFrameCounter_ < kReadbackInterval) {
      return;
    }
    readbackFrameCounter_ = 0;

    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();

    // freeListIndexResource_: UAV → COPY_SOURCE
    dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_SOURCE,
      freeListIndexResource_.Get());

    // UAV バッファ → Readback バッファにコピー（4バイトのみ）
    commandList->CopyBufferRegion(
      freeListIndexReadbackResource_.Get(), 0,
      freeListIndexResource_.Get(), 0,
      sizeof(int32_t));

    // freeListIndexResource_: COPY_SOURCE → UAV
    dx12_->TransitionResourceState(
      D3D12_RESOURCE_STATE_COPY_SOURCE,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      freeListIndexResource_.Get());

    // Readback バッファから CPU 読み取り（前回コピー分の結果、表示用途では問題なし）
    int32_t* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, sizeof(int32_t) };
    D3D12_RANGE writeRange = { 0, 0 };
    HRESULT hr = freeListIndexReadbackResource_->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
    if (SUCCEEDED(hr) && mappedData) {
      int32_t freeListIndex = *mappedData;
      int32_t active = static_cast<int32_t>(kNumMaxParticle) - 1 - freeListIndex;
      activeParticleCount_ = static_cast<uint32_t>((std::max)(0, active));
      freeListIndexReadbackResource_->Unmap(0, &writeRange);
    }
  }
#endif

  void GPUParticle::CreateIntegrateAllComputeRS()
  {
    HRESULT hr;
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange: UAV
    D3D12_DESCRIPTOR_RANGE rangeParticle[1] = {};
    rangeParticle[0].BaseShaderRegister = 0; // u0
    rangeParticle[0].NumDescriptors = 1;
    rangeParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeFreeListIndex[1] = {};
    rangeFreeListIndex[0].BaseShaderRegister = 1; // u1
    rangeFreeListIndex[0].NumDescriptors = 1;
    rangeFreeListIndex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeFreeListIndex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeFreeList[1] = {};
    rangeFreeList[0].BaseShaderRegister = 2; // u2
    rangeFreeList[0].NumDescriptors = 1;
    rangeFreeList[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangeFreeList[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // DescriptorRange: SRV
    D3D12_DESCRIPTOR_RANGE rangeForceFields[1] = {};
    rangeForceFields[0].BaseShaderRegister = 0; // t0
    rangeForceFields[0].NumDescriptors = 1;
    rangeForceFields[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeForceFields[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE rangeDepthBuffer[1] = {};
    rangeDepthBuffer[0].BaseShaderRegister = 1; // t1
    rangeDepthBuffer[0].NumDescriptors = 1;
    rangeDepthBuffer[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeDepthBuffer[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Emitter SRV (t2) — IntegrateAll で targetPosition/convergeStiffness 等を逆引き
    D3D12_DESCRIPTOR_RANGE rangeEmitters[1] = {};
    rangeEmitters[0].BaseShaderRegister = 2; // t2
    rangeEmitters[0].NumDescriptors = 1;
    rangeEmitters[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeEmitters[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // perEmitterCount UAV (u3) — 描画コンパクション用: per-emitter の今フレーム生存数
    D3D12_DESCRIPTOR_RANGE rangePerEmitterCount[1] = {};
    rangePerEmitterCount[0].BaseShaderRegister = 3; // u3
    rangePerEmitterCount[0].NumDescriptors = 1;
    rangePerEmitterCount[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangePerEmitterCount[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameter: 3 UAV + 3 SRV + 2 CBV + 1 UAV(perEmitterCount) = 9
    D3D12_ROOT_PARAMETER rootParameters[9] = {};

    // [0] Particles UAV (u0)
    rootParameters[IntegrateRP::kParticleUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kParticleUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kParticleUavParam].DescriptorTable.pDescriptorRanges = rangeParticle;
    rootParameters[IntegrateRP::kParticleUavParam].DescriptorTable.NumDescriptorRanges = 1;

    // [1] FreeListIndex UAV (u1)
    rootParameters[IntegrateRP::kFreeListIndexUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kFreeListIndexUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kFreeListIndexUavParam].DescriptorTable.pDescriptorRanges = rangeFreeListIndex;
    rootParameters[IntegrateRP::kFreeListIndexUavParam].DescriptorTable.NumDescriptorRanges = 1;

    // [2] FreeList UAV (u2)
    rootParameters[IntegrateRP::kFreeListUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kFreeListUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kFreeListUavParam].DescriptorTable.pDescriptorRanges = rangeFreeList;
    rootParameters[IntegrateRP::kFreeListUavParam].DescriptorTable.NumDescriptorRanges = 1;

    // [3] ForceFields SRV (t0)
    rootParameters[IntegrateRP::kForceFieldSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kForceFieldSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kForceFieldSrvParam].DescriptorTable.pDescriptorRanges = rangeForceFields;
    rootParameters[IntegrateRP::kForceFieldSrvParam].DescriptorTable.NumDescriptorRanges = 1;

    // [4] DepthBuffer SRV (t1) — 深度バッファ衝突用
    rootParameters[IntegrateRP::kDepthSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kDepthSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kDepthSrvParam].DescriptorTable.pDescriptorRanges = rangeDepthBuffer;
    rootParameters[IntegrateRP::kDepthSrvParam].DescriptorTable.NumDescriptorRanges = 1;

    // [5] PerFrame CBV (b0)
    rootParameters[IntegrateRP::kPerFrameCbvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[IntegrateRP::kPerFrameCbvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kPerFrameCbvParam].Descriptor.ShaderRegister = 0;

    // [6] PhysicsParams CBV (b1)
    rootParameters[IntegrateRP::kPhysicsParamsCbvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[IntegrateRP::kPhysicsParamsCbvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kPhysicsParamsCbvParam].Descriptor.ShaderRegister = 1;

    // [7] Emitter SRV (t2) — emitter 設定の逆引き用
    rootParameters[IntegrateRP::kEmitterSrvParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kEmitterSrvParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kEmitterSrvParam].DescriptorTable.pDescriptorRanges = rangeEmitters;
    rootParameters[IntegrateRP::kEmitterSrvParam].DescriptorTable.NumDescriptorRanges = 1;

    // [8] perEmitterCount UAV (u3) — per-emitter 生存数カウンタ
    rootParameters[IntegrateRP::kPerEmitterCountUavParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[IntegrateRP::kPerEmitterCountUavParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[IntegrateRP::kPerEmitterCountUavParam].DescriptorTable.pDescriptorRanges = rangePerEmitterCount;
    rootParameters[IntegrateRP::kPerEmitterCountUavParam].DescriptorTable.NumDescriptorRanges = 1;

    // Static Sampler: Point/Clamp（深度テクスチャサンプリング用）
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0; // s0
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }
    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(integrateAllRS_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void GPUParticle::CreateForceFieldResource()
  {
    // フォースフィールドリソースの生成（UPLOAD ヒープ — CPU から毎フレーム書き換え可能）
    dx12_->CreateBufferResource(forceFieldResource_, sizeof(ForceFieldData) * kMaxForceFields);

    // SRV を作成
    forceFieldSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(forceFieldSrvIndex_, forceFieldResource_.Get(), kMaxForceFields, sizeof(ForceFieldData));

    // フォースフィールドリストの初期化
    forceFields_.clear();

    // GPU 側のゼロ初期化
    ForceFieldData* gpuForceFields = nullptr;
    forceFieldResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuForceFields));
    ZeroMemory(gpuForceFields, sizeof(ForceFieldData) * kMaxForceFields);
    forceFieldResource_->Unmap(0, nullptr);
  }

  void GPUParticle::CreatePhysicsParamsResource()
  {
    // 物理パラメータの定数バッファを生成
    dx12_->CreateBufferResource(physicsParamsResource_, sizeof(PhysicsParamsData));

    // データをマップ
    physicsParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&physicsParamsData_));

    // デフォルト値の設定
    physicsParamsData_->depthBias = 5.0f;  ///< ワールド空間の最大衝突距離（メートル単位）
    physicsParamsData_->pad0[0] = 0.0f;
    physicsParamsData_->pad0[1] = 0.0f;
    physicsParamsData_->pad0[2] = 0.0f;

    physicsParamsData_->gridOrigin = { .x = -50.0f, .y = -50.0f, .z = -50.0f };
    physicsParamsData_->gridCellSize = 1.5625f; // 100.0 / 64.0
    physicsParamsData_->gridDimX = 64;
    physicsParamsData_->gridDimY = 64;
    physicsParamsData_->gridDimZ = 64;
    physicsParamsData_->activeForceFieldCount = 0;

    physicsParamsData_->invViewProj = Mat4x4::MakeIdentity();
    physicsParamsData_->screenWidth = 1280.0f;
    physicsParamsData_->screenHeight = 720.0f;
    physicsParamsData_->noiseTime = 0.0f;
    physicsParamsData_->pad1 = 0.0f;

    // 深度バッファ衝突用
    physicsParamsData_->viewProj = Mat4x4::MakeIdentity();
    physicsParamsData_->cameraPos = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
    physicsParamsData_->pad2 = 0.0f;
  }

  void GPUParticle::CreateDepthSRV()
  {
    // 既存の SRV を解放（リサイズ時の再作成対応）
    srvManager_->Free(depthSrvIndex_);
    depthSrvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForTexture2D(
      depthSrvIndex_,
      dx12_->GetDepthStencilResource(),
      DXGI_FORMAT_R32_FLOAT,
      1);
  }

  void GPUParticle::SyncForceFieldData()
  {
    // GPU 側のフォースフィールドバッファにマップ
    ForceFieldData* gpuForceFields = nullptr;
    forceFieldResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuForceFields));

    // フォースフィールドデータをコピー
    size_t count = std::min(forceFields_.size(), static_cast<size_t>(kMaxForceFields));
    if (count > 0) {
      std::memcpy(gpuForceFields, forceFields_.data(), sizeof(ForceFieldData) * count);
    }

    // 残りをゼロクリア
    if (count < kMaxForceFields) {
      ZeroMemory(&gpuForceFields[count], sizeof(ForceFieldData) * (kMaxForceFields - count));
    }

    forceFieldResource_->Unmap(0, nullptr);
  }

  void GPUParticle::UpdatePhysicsParams()
  {
    // アクティブなフォースフィールド数を更新
    physicsParamsData_->activeForceFieldCount = static_cast<uint32_t>(
      std::min(forceFields_.size(), static_cast<size_t>(kMaxForceFields)));

    // 時間を更新（Curl Noise 用）
    physicsParamsData_->noiseTime = FrameTimer::GetInstance()->GetGameTime();

    // --- カメラ行列の計算（深度衝突用） ---
    Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(
      { .x = 1.0f, .y = 1.0f, .z = 1.0f },
      camera_->GetRotate(), camera_->GetTranslate());
    Vector3 camPos = camera_->GetTranslate();

#ifdef _DEBUG
    if (isDebug_) {
      cameraMatrix = Mat4x4::MakeAffine(
        { .x = 1.0f, .y = 1.0f, .z = 1.0f },
        DebugCamera::GetInstance()->GetRotate(),
        DebugCamera::GetInstance()->GetTranslate());
      camPos = DebugCamera::GetInstance()->GetTranslate();
    }
#endif

    Matrix4x4 vp = Mat4x4::Multiply(
      Mat4x4::Inverse(cameraMatrix), camera_->GetProjectionMatrix());

    physicsParamsData_->viewProj = vp;
    physicsParamsData_->invViewProj = Mat4x4::Inverse(vp);
    physicsParamsData_->cameraPos = camPos;

    // スクリーンサイズ（リサイズ対応）
    physicsParamsData_->screenWidth = static_cast<float>(WinApp::clientWidth);
    physicsParamsData_->screenHeight = static_cast<float>(WinApp::clientHeight);
  }

  int32_t GPUParticle::AddForceField(const ForceFieldData& field)
  {
    if (forceFields_.size() >= kMaxForceFields) {
      return -1;
    }
    forceFields_.push_back(field);
    return static_cast<int32_t>(forceFields_.size() - 1);
  }

  void GPUParticle::UpdateForceField(uint32_t index, const ForceFieldData& field)
  {
    if (index < forceFields_.size()) {
      forceFields_[index] = field;
    }
  }

  void GPUParticle::RemoveForceField(uint32_t index)
  {
    if (index < forceFields_.size()) {
      forceFields_.erase(forceFields_.begin() + index);
    }
  }

  void GPUParticle::ClearForceFields()
  {
    forceFields_.clear();
  }

} // namespace Tako
