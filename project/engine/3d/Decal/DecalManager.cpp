#include "DecalManager.h"
#include "Decal.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "SrvManager.h"
#include "Mat4x4Func.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  std::unique_ptr<DecalManager> DecalManager::instance_ = nullptr;

  DecalManager* DecalManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<DecalManager>(Token{});
    }
    return instance_.get();
  }

  void DecalManager::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    CreateCubeMesh();
    CreateDepthSRV();
    CreatePipeline();
  }

  void DecalManager::Finalize()
  {
    // 深度 SRV を解放
    if (depthSrvIndex_ != 0) {
      SrvManager::GetInstance()->Free(depthSrvIndex_);
      depthSrvIndex_ = 0;
    }

    instance_.reset();
  }

  void DecalManager::OnResize()
  {
    // ウィンドウサイズが変更されたら深度 SRV を再作成
    CreateDepthSRV();
  }

  void DecalManager::BeginDraw(const Camera& camera, GpuResource& depthResource, uint32_t depthSrvIndex)
  {
    const DX12Basic::SceneRenderTarget& target = dx12_->GetSceneRenderTarget();

    // ViewData を視点ごとのスロットへ書き込む（画面サイズは描画先 RT のサイズ）
    ViewDataGPU viewData{};
    viewData.viewProj = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    viewData.invViewProj = Mat4x4::Inverse(viewData.viewProj);
    viewData.screenWidth = static_cast<float>(target.width);
    viewData.screenHeight = static_cast<float>(target.height);
    const D3D12_GPU_VIRTUAL_ADDRESS viewDataAddress = dx12_->GetUploadAllocator().Push(viewData);

    // 深度バッファを PIXEL_SHADER_RESOURCE に遷移
    dx12_->GetContext().Transition(depthResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 描画先 RT のビューポートを保ったまま、DSV を外して RTV だけ再バインド
    dx12_->RestoreSceneRenderTarget();
    CommandContext& context = dx12_->GetContext();
    context.SetRenderTarget(&target.rtv, target.rtvFormat, nullptr, DXGI_FORMAT_UNKNOWN);
    context.SetGraphicsPipeline(pipeline_);

    // キューブメッシュの VBV / IBV をバインド
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &cubeVBV_);
    commandList->IASetIndexBuffer(&cubeIBV_);

    CommandContext::ShaderBinder binder = context.Graphics();
    binder.BindCBV("gViewData"_id, viewDataAddress);
    binder.BindSRV("gDepthTexture"_id, depthSrvIndex);
  }

  void DecalManager::EndDraw(GpuResource& depthResource)
  {
    // 深度バッファを DEPTH_WRITE に復帰
    dx12_->GetContext().Transition(depthResource, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // シーン描画先（RTV + DSV）を再バインド
    dx12_->RestoreSceneRenderTarget();
  }

  void DecalManager::AddDecal(Decal* decal)
  {
    if (decal) {
      decals_.push_back(decal);
    }
  }

  void DecalManager::RemoveDecal(Decal* decal)
  {
    decals_.remove(decal);
  }

  void DecalManager::UpdateAll()
  {
    for (Decal* decal : decals_) {
      if (decal) {
        decal->Update();
      }
    }
  }

  void DecalManager::DrawAll()
  {
    DrawAllForView(*camera_, dx12_->GetDepthStencilResource(), depthSrvIndex_);
  }

  void DecalManager::DrawAllForView(const Camera& camera, GpuResource& depthResource, uint32_t depthSrvIndex)
  {
    if (decals_.empty()) return;

    BeginDraw(camera, depthResource, depthSrvIndex);
    for (Decal* decal : decals_) {
      if (decal) {
        decal->Draw();
      }
    }
    EndDraw(depthResource);
  }

  void DecalManager::DrawAllDebug()
  {
    if (decals_.empty()) return;

    for (Decal* decal : decals_) {
      if (decal) {
        decal->DrawDebug();
      }
    }
  }

  void DecalManager::ClearDecals()
  {
    decals_.clear();
  }

  // ルートシグネチャの作成
  void DecalManager::CreatePipeline()
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();
    pipeline_.vs = library.Get(L"Decal.VS.hlsl", ShaderStage::Vertex);
    pipeline_.ps = library.Get(L"Decal.PS.hlsl", ShaderStage::Pixel);
    pipeline_.vertexLayout = VertexLayout::Position;
    // キューブの内側に視点が入っても描けるよう両面を描き、深度は自前の深度テクスチャで判定する
    pipeline_.renderState = { .blend = BlendMode::Alpha, .cull = CullMode::None, .depthTest = false, .depthWrite = false };
  }

  void DecalManager::CreateCubeMesh()
  {
    // 8 頂点（float4: x, y, z, w）
    struct CubeVertex {
      float x, y, z, w;
    };

    CubeVertex vertices[8] = {
      { -0.5f, -0.5f, -0.5f, 1.0f }, // 0: 左下手前
      {  0.5f, -0.5f, -0.5f, 1.0f }, // 1: 右下手前
      {  0.5f,  0.5f, -0.5f, 1.0f }, // 2: 右上手前
      { -0.5f,  0.5f, -0.5f, 1.0f }, // 3: 左上手前
      { -0.5f, -0.5f,  0.5f, 1.0f }, // 4: 左下奥
      {  0.5f, -0.5f,  0.5f, 1.0f }, // 5: 右下奥
      {  0.5f,  0.5f,  0.5f, 1.0f }, // 6: 右上奥
      { -0.5f,  0.5f,  0.5f, 1.0f }, // 7: 左上奥
    };

    // 36 インデックス（12 三角形）
    uint16_t indices[36] = {
      // 手前面 (z-)
      0, 2, 1,  0, 3, 2,
      // 奥面 (z+)
      4, 5, 6,  4, 6, 7,
      // 左面 (x-)
      4, 7, 3,  4, 3, 0,
      // 右面 (x+)
      1, 2, 6,  1, 6, 5,
      // 下面 (y-)
      4, 0, 1,  4, 1, 5,
      // 上面 (y+)
      3, 7, 6,  3, 6, 2,
    };

    // 頂点バッファの作成
    cubeVertexBuffer_ = dx12_->MakeBufferResource(sizeof(vertices));
    CubeVertex* vertexData = nullptr;
    cubeVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices, sizeof(vertices));
    cubeVertexBuffer_->Unmap(0, nullptr);

    cubeVBV_.BufferLocation = cubeVertexBuffer_->GetGPUVirtualAddress();
    cubeVBV_.SizeInBytes = sizeof(vertices);
    cubeVBV_.StrideInBytes = sizeof(CubeVertex);

    // インデックスバッファの作成
    cubeIndexBuffer_ = dx12_->MakeBufferResource(sizeof(indices));
    uint16_t* indexData = nullptr;
    cubeIndexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    memcpy(indexData, indices, sizeof(indices));
    cubeIndexBuffer_->Unmap(0, nullptr);

    cubeIBV_.BufferLocation = cubeIndexBuffer_->GetGPUVirtualAddress();
    cubeIBV_.SizeInBytes = sizeof(indices);
    cubeIBV_.Format = DXGI_FORMAT_R16_UINT;
  }

  // 深度 SRV の作成
  void DecalManager::CreateDepthSRV()
  {
    // 前回の SRV があれば解放
    if (depthSrvIndex_ != 0) {
      SrvManager::GetInstance()->Free(depthSrvIndex_);
    }

    depthSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      depthSrvIndex_,
      dx12_->GetDepthStencilResource().Get(),
      DXGI_FORMAT_R32_FLOAT,
      1
    );
  }

} // namespace Tako
