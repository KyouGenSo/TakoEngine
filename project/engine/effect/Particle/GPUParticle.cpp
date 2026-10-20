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
#include "Model.h"
#include "Mesh.h"
#include "ModelManager.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif // _DEBUG

#include <numbers>

namespace Tako {

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

    CreatePipelines();

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
#ifdef _DEBUG
#endif

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
    CommandContext& context = dx12_->GetContext();

    /// ================================== ///
    ///            ComputerShader          ///
    /// ================================== ///

    //--------------------------------------初期化--------------------------------------//
    if (!isInited_) {
      context.SetComputePipeline(initPipeline_);
      CommandContext::ShaderBinder binder = context.Compute();
      binder.BindUAV("gParticles"_id, particleUavIndex_);
      binder.BindUAV("gFreeListIndex"_id, freeListIndexUavIndex_);
      binder.BindUAV("gFreeList"_id, freeListUavIndex_);
      context.Dispatch(1024, 1, 1);

      isInited_ = true;
    }

    // リソースバリアの設定（UAV 同期）
    context.UavBarrier(particleResource_);
    context.UavBarrier(freeListIndexResource_);
    context.UavBarrier(freeListResource_);

    //--------------------------------------射出--------------------------------------//
      // アクティブなエミッターがある場合のみ実行
    if (!activeEmitters_.empty()) {
      context.SetComputePipeline(emitPipeline_);
      CommandContext::ShaderBinder binder = context.Compute();
      binder.BindUAV("gParticles"_id, particleUavIndex_);
      binder.BindUAV("gFreeListIndex"_id, freeListIndexUavIndex_);
      binder.BindUAV("gFreeList"_id, freeListUavIndex_);
      binder.BindSRV("gEmitters"_id, emitterSrvIndex_);
      binder.BindCBV("gPerFrame"_id, perFrameResource_->GetGPUVirtualAddress());

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
        binder.BindConstants("RootConstants"_id, kInvalidMeshTarget);
        binder.BindSRV("gMeshVertices"_id, emitterSrvIndex_);
        binder.BindSRV("gMeshIndices"_id, emitterSrvIndex_);
        binder.BindSRV("gMeshAreaPrefixSum"_id, emitterSrvIndex_);
        context.Dispatch(threadGroupsX, 1, 1);
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

        binder.BindSRV("gMeshVertices"_id, vtxSrv != 0 ? vtxSrv : emitterSrvIndex_);
        binder.BindSRV("gMeshIndices"_id, edata.meshIndexSrvIndex != 0 ? edata.meshIndexSrvIndex : emitterSrvIndex_);
        binder.BindSRV("gMeshAreaPrefixSum"_id, edata.meshAreaPrefixSumSrvIndex != 0 ? edata.meshAreaPrefixSumSrvIndex : emitterSrvIndex_);
        binder.BindConstants("RootConstants"_id, i);
        context.Dispatch(1, 1, 1);
      }
    }

    // リソースバリアの設定（UAV 同期）
    context.UavBarrier(particleResource_);
    context.UavBarrier(freeListIndexResource_);
    context.UavBarrier(freeListResource_);

    //--------------------------------------per-emitter 生存数カウンタのリセット--------------------------------------//
    // IntegrateAll が InterlockedAdd で加算する前に perEmitterCount[0..kNumMaxEmitter) を 0 クリアする
    context.SetComputePipeline(resetCountersPipeline_);
    context.Compute().BindUAV("gPerEmitterCount"_id, perEmitterCountUavIndex_);
    context.Dispatch(1, 1, 1); // numthreads(1024) で kNumMaxEmitter をカバー
    context.UavBarrier(perEmitterCountResource_);

    //--------------------------------------IntegrateAll--------------------------------------//

    // 深度バッファを NON_PIXEL_SHADER_RESOURCE に遷移（深度衝突用）
    context.Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    context.SetComputePipeline(integratePipeline_);
    {
      CommandContext::ShaderBinder binder = context.Compute();
      binder.BindUAV("gParticles"_id, particleUavIndex_);
      binder.BindUAV("gFreeListIndex"_id, freeListIndexUavIndex_);
      binder.BindUAV("gFreeList"_id, freeListUavIndex_);
      binder.BindSRV("gForceFields"_id, forceFieldSrvIndex_);
      binder.BindSRV("gDepthBuffer"_id, depthSrvIndex_);  // 深度バッファ衝突用
      binder.BindCBV("gPerFrame"_id, perFrameResource_->GetGPUVirtualAddress());
      binder.BindCBV("gPhysicsParams"_id, physicsParamsResource_->GetGPUVirtualAddress());
      binder.BindSRV("gEmitters"_id, emitterSrvIndex_);  // targetPosition 等の参照
      binder.BindUAV("gPerEmitterCount"_id, perEmitterCountUavIndex_);  // per-emitter 生存数のカウント先
    }

    // ディスパッチ（256スレッド/グループ × ceil(1M/256) = 3907グループ）
    uint32_t integrateGroups = (kNumMaxParticle + 255) / 256;
    context.Dispatch(integrateGroups, 1, 1);

    context.UavBarrier(particleResource_);
    context.UavBarrier(perEmitterCountResource_); // BuildDrawArgs が読む前に per-emitter 生存数の書き込みを同期

#ifdef _DEBUG
    // アクティブパーティクル数の Readback
    ReadbackActiveParticleCount();
#endif

    // 深度バッファを DEPTH_WRITE に復帰
    context.Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

    //--------------------------------------BuildDrawArgs--------------------------------------//
    // per-emitter 生存数を排他プレフィックスサムし、per-emitter の Indirect 引数(base_e/count_e) と
    // スキャッタ用カーソル(=base_e) を構築する
    context.SetComputePipeline(buildDrawArgsPipeline_);
    {
      CommandContext::ShaderBinder binder = context.Compute();
      binder.BindUAV("gPerEmitterCount"_id, perEmitterCountUavIndex_);
      binder.BindUAV("gDrawArgs"_id, drawArgsUavIndex_);
      binder.BindUAV("gScatterCursor"_id, scatterCursorUavIndex_);
      binder.BindSRV("gEmitterTemplate"_id, emitterIndexCountSrvIndex_);
    }
    context.Dispatch(1, 1, 1);
    context.UavBarrier(drawArgsResource_);
    context.UavBarrier(scatterCursorResource_);

    //--------------------------------------ScatterCompact--------------------------------------//
    // 散在する生存パーティクル index を drawIndexList に先頭から詰め直す
    // (particleResource_ は IntegrateAll 直後の UAV state のまま読む)
    context.SetComputePipeline(scatterCompactPipeline_);
    {
      CommandContext::ShaderBinder binder = context.Compute();
      binder.BindUAV("gParticles"_id, particleUavIndex_);
      binder.BindUAV("gDrawIndexList"_id, drawIndexUavIndex_);
      binder.BindUAV("gScatterCursor"_id, scatterCursorUavIndex_);
    }
    {
      uint32_t scatterGroups = (kNumMaxParticle + 255) / 256;
      context.Dispatch(scatterGroups, 1, 1);
    }
    context.UavBarrier(drawIndexResource_);

    /// ======================== ///
    ///           描画    　     ///
    /// ======================= ///

    DrawParticleGraphics(perViewResource_->GetGPUVirtualAddress(), -1);
  }

  void GPUParticle::DrawParticleGraphics(D3D12_GPU_VIRTUAL_ADDRESS perViewAddress, int32_t slotFilter)
  {
    CommandContext& context = dx12_->GetContext();

    // ParticleData は VS で SRV(t0) として読むため NON_PIXEL へ遷移
    context.Transition(particleResource_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    // drawIndexList は per-instance VBV として使うため VERTEX_AND_CONSTANT_BUFFER へ遷移
    context.Transition(drawIndexResource_, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    // DrawArgs を ExecuteIndirect の引数 state へ遷移
    context.Transition(drawArgsResource_, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // per-instance パーティクル index ストリーム (slot1) のみバインド。
    // 頂点/インデックスは VS が SRV(t3/t4) からプルするため slot0 頂点バッファ・IBV は不要。
    dx12_->GetCommandList()->IASetVertexBuffers(1, 1, &drawIndexVBV_);

    // 全エミッター共通の値。ブレンドだけが違うパイプラインはルートシグネチャを共有するので、ループ中も渡し直さなくてよい
    context.SetGraphicsPipeline(drawPipeline_);
    CommandContext::ShaderBinder binder = context.Graphics();
    binder.BindSRV("gParticles"_id, particleSrvIndex_);
    binder.BindCBV("gPerView"_id, perViewAddress);
    binder.BindSRV("gEmitters"_id, emitterSrvIndex_);  // ビルボード判定用

    // per-emitter ループ: エミッターごとにブレンドモード/テクスチャ/
    // 描画モデル(頂点 SRV t3・index SRV t4) を切り替えて 1 つずつ ExecuteIndirect。
    // instanceCount / StartInstanceLocation(=base_e) / VertexCountPerInstance(=描画モデル index 数) は
    // GPU 側 (BuildDrawArgs) 計算済み。描画モデル未指定のエミッターはデフォルト板ポリ SRV にフォールバックする。
    const size_t emitterCount = std::min(activeEmitters_.size(), static_cast<size_t>(kNumMaxEmitter));
    const UINT drawStride = static_cast<UINT>(sizeof(D3D12_DRAW_ARGUMENTS));
    GraphicsPipelineDesc pipeline = drawPipeline_;
    for (size_t i = 0; i < emitterCount; ++i) {
      if (slotFilter >= 0 && static_cast<int32_t>(i) != slotFilter) continue;
      const auto& emitter = activeEmitters_[i];
      if (!emitter) continue;
      const EmitterData& ed = emitter->GetData();
      const uint32_t texIndex = (ed.textureSrvIndex != 0) ? ed.textureSrvIndex : modelData_.textureData.textureIndex;
      const uint32_t vtxSrv = (ed.renderVertexSrvIndex != 0) ? ed.renderVertexSrvIndex : defaultQuadVertexSrvIndex_;
      const uint32_t idxSrv = (ed.renderIndexSrvIndex != 0) ? ed.renderIndexSrvIndex : defaultQuadIndexSrvIndex_;

      pipeline.renderState.blend = static_cast<BlendMode>(ed.blendMode);
      context.SetGraphicsPipeline(pipeline);
      binder.BindSRV("gTexture"_id, texIndex);
      binder.BindSRV("gMeshVertices"_id, vtxSrv);
      binder.BindSRV("gMeshIndices"_id, idxSrv);
      context.ExecuteIndirect(drawCommandSignature_.Get(), 1, drawArgsResource_, static_cast<UINT64>(i) * drawStride);
    }

    // 各リソースの state を UAV に戻す
    context.Transition(drawArgsResource_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.Transition(drawIndexResource_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.Transition(particleResource_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }

#ifdef _DEBUG
  void GPUParticle::DrawForCamera(const Camera& camera, int32_t slotFilter)
  {
    // UpdatePerView と同式を指定カメラで計算
    const Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f, .y = 1.0f, .z = 1.0f }, camera.GetRotate(), camera.GetTranslate());

    Matrix4x4 billboardMatrix = Mat4x4::Multiply(Mat4x4::MakeRotateY(std::numbers::pi_v<float>), cameraMatrix);
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    PerView perView{};
    perView.viewProjection = Mat4x4::Multiply(Mat4x4::Inverse(cameraMatrix), camera.GetProjectionMatrix());
    perView.billboardMatrix = billboardMatrix;

    DrawParticleGraphics(dx12_->GetUploadAllocator().Push(perView), slotFilter);
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

    const Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ .x = 1.0f,.y = 1.0f,.z = 1.0f }, camera_->GetRotate(), camera_->GetTranslate());

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

  void GPUParticle::CreatePipelines()
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();

    // 深度テストのみ行い、書き込まない
    drawPipeline_.vs = library.Get(L"GPUParticle.VS.hlsl", ShaderStage::Vertex);
    drawPipeline_.ps = library.Get(L"GPUParticle.PS.hlsl", ShaderStage::Pixel);
    drawPipeline_.vertexLayout = VertexLayout::ParticleInstance;
    drawPipeline_.renderState.blend = BlendMode::Add;
    drawPipeline_.renderState.depthWrite = false;

    initPipeline_.cs = library.Get(L"InitParticle.CS.hlsl", ShaderStage::Compute);
    emitPipeline_.cs = library.Get(L"EmitParticle.CS.hlsl", ShaderStage::Compute);
    integratePipeline_.cs = library.Get(L"IntegrateAll.CS.hlsl", ShaderStage::Compute);
    resetCountersPipeline_.cs = library.Get(L"ResetCounters.CS.hlsl", ShaderStage::Compute);
    buildDrawArgsPipeline_.cs = library.Get(L"BuildDrawArgs.CS.hlsl", ShaderStage::Compute);
    scatterCompactPipeline_.cs = library.Get(L"ScatterCompact.CS.hlsl", ShaderStage::Compute);
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
    particleResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

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
    freeListIndexResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    // FreeListIndex の UAV を生成
    freeListIndexUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(freeListIndexUavIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));


    // FreeList のリソースを生成
    dx12_->CreateResourceForUAV(freeListResource_, sizeof(uint32_t) * kNumMaxParticle);
    freeListResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    // FreeList の UAV を生成
    freeListUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(freeListUavIndex_, freeListResource_.Get(), kNumMaxParticle, sizeof(uint32_t));
  }

  void GPUParticle::CreateIndirectResources()
  {
    // --- per-emitter 生存数カウンタ (uint kNumMaxEmitter 要素) ---
    dx12_->CreateResourceForUAV(perEmitterCountResource_, sizeof(uint32_t) * kNumMaxEmitter);
    perEmitterCountResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    perEmitterCountUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(perEmitterCountUavIndex_, perEmitterCountResource_.Get(), kNumMaxEmitter, sizeof(uint32_t));

    // --- コンパクション済み生存 index 配列 (uint kNumMaxParticle 要素) ---
    dx12_->CreateResourceForUAV(drawIndexResource_, sizeof(uint32_t) * kNumMaxParticle);
    drawIndexResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
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
    scatterCursorResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    scatterCursorUavIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAV(scatterCursorUavIndex_, scatterCursorResource_.Get(), kNumMaxEmitter, sizeof(uint32_t));

    // --- per-emitter Indirect 描画引数 (D3D12_DRAW_ARGUMENTS kNumMaxEmitter 要素) ---
    dx12_->CreateResourceForUAV(drawArgsResource_, sizeof(D3D12_DRAW_ARGUMENTS) * kNumMaxEmitter);
    drawArgsResource_.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
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

    dx12_->GetContext().Transition(freeListIndexResource_, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // UAV バッファ → Readback バッファにコピー（4バイトのみ）
    dx12_->GetCommandList()->CopyBufferRegion(
      freeListIndexReadbackResource_.Get(), 0,
      freeListIndexResource_.Get(), 0,
      sizeof(int32_t));

    dx12_->GetContext().Transition(freeListIndexResource_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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
      dx12_->GetDepthStencilResource().Get(),
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
    const Matrix4x4 cameraMatrix = Mat4x4::MakeAffine(
      { .x = 1.0f, .y = 1.0f, .z = 1.0f },
      camera_->GetRotate(), camera_->GetTranslate());
    const Vector3 camPos = camera_->GetTranslate();

    Matrix4x4 vp = Mat4x4::Multiply(
      Mat4x4::Inverse(cameraMatrix), camera_->GetProjectionMatrix());

    physicsParamsData_->viewProj = vp;
    physicsParamsData_->invViewProj = Mat4x4::Inverse(vp);
    physicsParamsData_->cameraPos = camPos;

    // 衝突判定で読む深度バッファの解像度（解像度スケール適用後）
    physicsParamsData_->screenWidth = static_cast<float>(dx12_->GetSceneWidth());
    physicsParamsData_->screenHeight = static_cast<float>(dx12_->GetSceneHeight());
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
