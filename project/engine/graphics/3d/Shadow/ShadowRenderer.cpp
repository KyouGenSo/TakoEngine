#include "ShadowRenderer.h"
#include "Camera.h"
#include "DX12Basic.h"
#include "Light.h"
#include "Logger.h"
#include "SrvManager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <numbers>
#include "Mat4x4Func.h"

#ifdef _DEBUG
#include "EditorUI.h"
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

namespace {
    constexpr uint32_t kDefaultLocalShadowMapSize = 2048;

    constexpr float kGuardTexels      = 6.0f;                                   ///< 区画の縁に残す余白。PCF の幅とバイアスのずれが、区画の外を読まないようにする
    constexpr float kRadiusStep       = 1.0f / 16.0f;                           ///< カスケードの半径を丸める刻み（計算の誤差で、毎フレーム大きさが変わらないように）
    constexpr float kLocalNearClip    = 0.1f;                                   ///< スポットライト・点光源の影の near
    constexpr float kMaxSpotHalfAngle = 80.0f * std::numbers::pi_v<float> / 180.0f;  ///< これより広いコーンは、影の視野をここで止める（外側は影なし）
    constexpr float kFadeEpsilon      = 0.01f;

    // 点光源の 6 面の向き。Shadow.hlsli の PointShadow の面の番号と同じ並び
    constexpr Vector3 kPointFaceDirections[] = {
        { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
    };

    /// <summary>
    /// position に置いて direction（正規化済み）を向くライトのワールド行列。逆行列がビュー行列になる
    /// </summary>
    Matrix4x4 MakeLightWorld(const Vector3& position, const Vector3& direction)
    {
        // XZ 平面での向きを Y 軸まわりの角度に、上下の傾きを X 軸まわりの角度にする
        const float yaw   = std::atan2(direction.x, direction.z);
        const float pitch = std::atan2(-direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
        return Mat4x4::MakeAffine({ 1.0f, 1.0f, 1.0f }, Vector3{ pitch, yaw, 0.0f }, position);
    }

    /// <summary>
    /// viewProj の列 column の成分に sign を掛けて w の列へ足した面（内側が正）を、法線の長さ 1 にして返す
    /// </summary>
    Vector4 MakePlane(const Matrix4x4& viewProj, int column, float sign, bool addW)
    {
        Vector4 plane{};
        float* elements[] = { &plane.x, &plane.y, &plane.z, &plane.w };
        for (int row = 0; row < 4; ++row) {
            *elements[row] = viewProj.m[row][column] * sign + (addW ? viewProj.m[row][3] : 0.0f);
        }
        const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        return length > 0.0f ? plane / length : plane;
    }
}

// 静的メンバ変数の定義
std::unique_ptr<ShadowRenderer> ShadowRenderer::instance_ = nullptr;

ShadowRenderer* ShadowRenderer::GetInstance()
{
    if (!instance_)
    {
        instance_ = std::make_unique<ShadowRenderer>(Token{});
    }
    return instance_.get();
}

void ShadowRenderer::Initialize(DX12Basic* dx12)
{
    assert(dx12);

    dx12_ = dx12;

    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dx12_);

    localShadowMap_ = std::make_unique<ShadowMap>();
    localShadowMap_->SetShadowMapSize(kDefaultLocalShadowMapSize);
    localShadowMap_->Initialize(dx12_);

    CreateConstantBuffer();
}

void ShadowRenderer::Update()
{
    if (shadowMap_) {
        shadowMap_->BeginFrame();
    }
    if (localShadowMap_) {
        localShadowMap_->BeginFrame();
    }
}

void ShadowRenderer::Finalize()
{
    if (shadowConstantBuffer_ && shadowConstantData_) {
        shadowConstantBuffer_->Unmap(0, nullptr);
        shadowConstantData_ = nullptr;
    }

    if (shadowMap_) {
        shadowMap_->Finalize();
        shadowMap_.reset();
    }
    if (localShadowMap_) {
        localShadowMap_->Finalize();
        localShadowMap_.reset();
    }

    // インスタンスを削除
    instance_.reset();
}

void ShadowRenderer::Prepare()
{
    if (!shadowConstantData_) {
        return;
    }

    views_.clear();
    constants_.enableShadow = 0;
    if (shadowEnabled_ && camera_ && light_) {
        constants_.enableShadow    = 1;
        constants_.cameraPosition  = camera_->GetTranslate();
        constants_.atlasTexel      = { 1.0f / static_cast<float>(shadowMap_->GetShadowMapSize()), 1.0f / static_cast<float>(shadowMap_->GetShadowMapSize()) };
        constants_.localAtlasTexel = { 1.0f / static_cast<float>(localShadowMap_->GetShadowMapSize()), 1.0f / static_cast<float>(localShadowMap_->GetShadowMapSize()) };
        constants_.pcfKernelSize   = shadowMap_->GetPCFKernelSize();
        constants_.debugCascades   = showCascades_ ? 1 : 0;
        constants_.depthBias       = depthBias_;
        constants_.normalBias      = normalBias_;
        PrepareCascades();
        PrepareLocalLights();
    }
    else {
        // 影を止めている間はアトラスを描かないので、再開したフレームに全部描き直す
        cascadesDirty_ = true;
    }

    std::memcpy(shadowConstantData_, &constants_, sizeof(ShadowConstants));
    ++frameIndex_;
}

void ShadowRenderer::PrepareCascades()
{
    const uint32_t atlasSize = shadowMap_->GetShadowMapSize();
    const uint32_t tileSize  = cascadeCount_ > 1 ? atlasSize / kCascadeGrid : atlasSize;
    const bool     updateAll = cascadesDirty_ || !staggerFarCascades_ || cascadeCount_ != lastCascadeCount_ || tileSize != lastCascadeTile_;
    cascadesDirty_    = false;
    lastCascadeCount_ = cascadeCount_;
    lastCascadeTile_  = tileSize;

    const Vector3   direction = light_->GetDirectionalLight().direction.Normalize();
    const Matrix4x4 basis     = MakeLightWorld({}, direction);
    const Vector3   right     = Mat4x4::TransformNormal(basis, { 1.0f, 0.0f, 0.0f });
    const Vector3   up        = Mat4x4::TransformNormal(basis, { 0.0f, 1.0f, 0.0f });

    const float nearZ = camera_->GetNearClip();
    const float farZ  = (std::max)((std::min)(maxShadowDistance_, camera_->GetFarClip()), nearZ + kFadeEpsilon);
    constants_.cascadeCount = cascadeCount_;
    constants_.maxDistance  = farZ;

    const float tile      = static_cast<float>(tileSize);
    float       sliceNear = nearZ;
    for (int32_t i = 0; i < cascadeCount_; ++i) {
        // 等間隔と対数の間隔を cascadeSplit_ で混ぜる（対数に寄せるほど、手前のカスケードが狭く細かくなる）
        const float ratio       = static_cast<float>(i + 1) / static_cast<float>(cascadeCount_);
        const float uniform     = nearZ + (farZ - nearZ) * ratio;
        const float logarithmic = nearZ * std::pow(farZ / nearZ, ratio);
        const float sliceFar    = uniform + (logarithmic - uniform) * cascadeSplit_;

        // 3 枚目以降は 1 フレームおきに描き直す。描かないフレームは、前に描いた区画と定数をそのまま使う
        if (updateAll || i < 2 || (frameIndex_ + static_cast<uint32_t>(i)) % 2 == 0) {
            const std::array<Vector3, 8> corners = camera_->GetFrustumCorners(sliceNear, sliceFar);
            Vector3 center{};
            for (const Vector3& corner : corners) {
                center += corner;
            }
            center /= static_cast<float>(corners.size());

            // 切片を囲む球に合わせる。カメラが回っても正射影の大きさが変わらないので、影の縁がちらつかない
            float radius = 0.0f;
            for (const Vector3& corner : corners) {
                radius = (std::max)(radius, (corner - center).Length());
            }
            radius = std::ceil(radius / kRadiusStep) * kRadiusStep;

            const float halfSize   = radius * tile / (tile - kGuardTexels * 2.0f);
            const float texelWorld = halfSize * 2.0f / tile;

            // 正射影の中心をテクセルの倍数に丸める。カメラが動いても、影の縁が同じテクセルに乗り続ける
            const float   x       = center.Dot(right);
            const float   y       = center.Dot(up);
            const Vector3 snapped = center + right * (std::floor(x / texelWorld) * texelWorld - x) + up * (std::floor(y / texelWorld) * texelWorld - y);

            const Matrix4x4 view = Mat4x4::Inverse(MakeLightWorld(snapped - direction * halfSize, direction));
            const Matrix4x4 proj = Mat4x4::MakeOrtho(-halfSize, halfSize, halfSize, -halfSize, 0.0f, halfSize * 2.0f);
            AddSlice(static_cast<uint32_t>(i), view * proj, false, static_cast<uint32_t>(i), texelWorld, false);
            constants_.cascadeSpheres[i] = { center.x, center.y, center.z, radius * radius };
        }
        sliceNear = sliceFar;
    }
}

void ShadowRenderer::PrepareLocalLights()
{
    const std::span<const Light::ShadowRequest> requests = light_->GetShadowRequests();
    if (requests.empty()) {
        return;
    }

    // 区画が足りないときに近いライトを残すため、照らす範囲の縁がカメラに近い順に並べる
    const Vector3 cameraPosition = camera_->GetTranslate();
    const auto nearness = [&](uint32_t index) { return (requests[index].position - cameraPosition).Length() - requests[index].range; };
    requestOrder_.resize(requests.size());
    for (uint32_t i = 0; i < requestOrder_.size(); ++i) {
        requestOrder_[i] = i;
    }
    std::ranges::sort(requestOrder_, [&](uint32_t a, uint32_t b) { return nearness(a) < nearness(b); });

    const float tile = static_cast<float>(localShadowMap_->GetShadowMapSize() / kLocalGrid);
    // 点光源の面は、90° の範囲を区画の縁から kGuardTexels 内側に収まるよう視野を広げる（面の境目で PCF が切れないように）
    const float faceTanHalf = tile / (tile - kGuardTexels * 2.0f);

    uint32_t used = 0;
    for (const uint32_t index : requestOrder_) {
        const Light::ShadowRequest& request = requests[index];
        const uint32_t              count   = request.isSpot ? 1 : kPointLightFaces;
        if (used + count > kMaxLocalSlices) {
            if (!overflowWarned_) {
                Logger::Log("ShadowRenderer: more shadow-casting point / spot lights than the %u shadow slots (spot = 1, point = 6). Lights far from the camera cast no shadow\n", kMaxLocalSlices);
                overflowWarned_ = true;
            }
            continue;
        }

        const float farClip = (std::max)(request.range, kLocalNearClip * 2.0f);
        if (request.isSpot) {
            const float     halfAngle = (std::min)(std::acos(std::clamp(request.cosAngle, -1.0f, 1.0f)), kMaxSpotHalfAngle);
            const Matrix4x4 view      = Mat4x4::Inverse(MakeLightWorld(request.position, request.direction.Normalize()));
            const Matrix4x4 proj      = Mat4x4::MakePerspective(halfAngle * 2.0f, 1.0f, kLocalNearClip, farClip);
            AddSlice(kFirstLocalSlice + used, view * proj, true, used, std::tan(halfAngle) * 2.0f / tile, true);
        }
        else {
            const Matrix4x4 proj = Mat4x4::MakePerspective(std::atan(faceTanHalf) * 2.0f, 1.0f, kLocalNearClip, farClip);
            for (uint32_t face = 0; face < kPointLightFaces; ++face) {
                const Matrix4x4 view = Mat4x4::Inverse(MakeLightWorld(request.position, kPointFaceDirections[face]));
                AddSlice(kFirstLocalSlice + used + face, view * proj, true, used + face, faceTanHalf * 2.0f / tile, true);
            }
        }
        light_->SetShadowSlice(request, static_cast<int32_t>(kFirstLocalSlice + used));
        used += count;
    }
}

void ShadowRenderer::AddSlice(uint32_t sliceIndex, const Matrix4x4& viewProj, bool isLocal, uint32_t tileIndex, float texelWorld, bool perspective)
{
    const uint32_t atlasSize = (isLocal ? localShadowMap_ : shadowMap_)->GetShadowMapSize();
    const uint32_t grid      = isLocal ? kLocalGrid : (cascadeCount_ > 1 ? kCascadeGrid : 1);
    const uint32_t tileSize  = atlasSize / grid;

    ShadowView view;
    view.isLocal = isLocal;
    view.x       = (tileIndex % grid) * tileSize;
    view.y       = (tileIndex / grid) * tileSize;
    view.size    = tileSize;
    view.planes  = { MakePlane(viewProj, 0, 1.0f, true),  MakePlane(viewProj, 0, -1.0f, true), MakePlane(viewProj, 1, 1.0f, true),
                     MakePlane(viewProj, 1, -1.0f, true), MakePlane(viewProj, 2, -1.0f, true), MakePlane(viewProj, 2, 1.0f, false) };
    // 平行光源は、範囲よりライト側にある物も影を落とす（near 面へ押し付けて描く）ので、手前の面では捨てない
    view.planeCount = perspective ? 6 : 5;
    view.constants  = dx12_->GetUploadAllocator().Push(ShadowCasterConstants{ .lightViewProj = viewProj, .pancake = perspective ? 0.0f : 1.0f });
    views_.push_back(view);

    // クリップ空間（x, y は -1〜1、y は上が正）を、アトラスの中の区画の UV へ移す
    const float     atlas   = static_cast<float>(atlasSize);
    const float     scale   = static_cast<float>(tileSize) / atlas;
    const float     offsetX = static_cast<float>(view.x) / atlas;
    const float     offsetY = static_cast<float>(view.y) / atlas;
    const Matrix4x4 toAtlas = { {
        { scale * 0.5f,           0.0f,                   0.0f, 0.0f },
        { 0.0f,                   -scale * 0.5f,          0.0f, 0.0f },
        { 0.0f,                   0.0f,                   1.0f, 0.0f },
        { scale * 0.5f + offsetX, scale * 0.5f + offsetY, 0.0f, 1.0f },
    } };
    const float halfTexel = 0.5f / atlas;

    ShadowSlice& slice = constants_.slices[sliceIndex];
    slice.worldToAtlas = viewProj * toAtlas;
    slice.uvBounds     = { offsetX + halfTexel, offsetY + halfTexel, offsetX + scale - halfTexel, offsetY + scale - halfTexel };
    slice.params       = { texelWorld, perspective ? 1.0f : 0.0f, 0.0f, 0.0f };
}

void ShadowRenderer::BeginView(const ShadowView& view)
{
    isRenderingShadow_ = true;
    (view.isLocal ? localShadowMap_ : shadowMap_)->BeginTile(view.x, view.y, view.size);
}

void ShadowRenderer::EndShadowPass()
{
    shadowMap_->EndShadowMapRender();
    localShadowMap_->EndShadowMapRender();
    isRenderingShadow_ = false;

    // 本編/デバッグビュー/プレビューのどれから呼ばれても、パス開始前の描画先へ戻す
    dx12_->RestoreSceneRenderTarget();
}

void ShadowRenderer::SetShadowForMainPass(bool receiveShadow)
{
    if (isRenderingShadow_) {
        return;
    }
    CommandContext::ShaderBinder binder = dx12_->GetContext().Graphics();
    if (shadowConstantBuffer_) {
        binder.BindCBV("gShadowConstants"_id, (receiveShadow ? shadowConstantBuffer_ : disabledConstantBuffer_)->GetGPUVirtualAddress());
    }
    if (shadowMap_) {
        binder.BindSRV("gShadowMap"_id, shadowMap_->GetSrvIndex());
    }
    if (localShadowMap_) {
        binder.BindSRV("gLocalShadowMap"_id, localShadowMap_->GetSrvIndex());
    }
}

void ShadowRenderer::CreateConstantBuffer()
{
    shadowConstantBuffer_ = dx12_->MakeBufferResource(sizeof(ShadowConstants));
    shadowConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&shadowConstantData_));
    std::memcpy(shadowConstantData_, &constants_, sizeof(ShadowConstants));

    // enableShadow = 0 のまま変えないので、書いたら閉じる
    disabledConstantBuffer_ = dx12_->MakeBufferResource(sizeof(ShadowConstants));
    void* disabled = nullptr;
    disabledConstantBuffer_->Map(0, nullptr, &disabled);
    std::memset(disabled, 0, sizeof(ShadowConstants));
    disabledConstantBuffer_->Unmap(0, nullptr);
}

void ShadowRenderer::SetShadowQuality(int quality)
{
    if (shadowMap_) {
        shadowMap_->SetShadowQuality(static_cast<ShadowMap::ShadowQuality>(quality));
    }
}

void ShadowRenderer::SetShadowMapSize(uint32_t size)
{
    if (shadowMap_) {
        shadowMap_->SetShadowMapSize(size);
    }
}

void ShadowRenderer::SetLocalShadowMapSize(uint32_t size)
{
    if (localShadowMap_) {
        localShadowMap_->SetShadowMapSize(size);
    }
}

void ShadowRenderer::SetPCFKernelSize(int kernelSize)
{
    if (shadowMap_) {
        shadowMap_->SetPCFKernelSize(kernelSize);
    }
}

void ShadowRenderer::SetCascadeCount(int32_t count)
{
    cascadeCount_ = std::clamp(count, 1, kMaxCascades);
}

void ShadowRenderer::SetCascadeSplit(float split)
{
    cascadeSplit_ = std::clamp(split, 0.0f, 1.0f);
}

void ShadowRenderer::DrawImGui()
{
#ifdef _DEBUG
    UI::Checkbox("Enable Shadow", &shadowEnabled_);

    // シャドウ品質プリセット
    ImGui::Separator();
    ImGui::Text("Shadow Quality");
    static int shadowQuality = 2; // デフォルトは High
    const char* qualityNames[] = { "Low (512x512, NO PCF)", "Medium (1024x1024, PCF 3x3)",
                                    "High (2048x2048, PCF 5x5)", "Ultra (4096x4096, PCF 7x7)",
                                    "Super (8192x8192, PCF 7x7)" };
    if (UI::Combo("Quality Preset", &shadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames))) {
        SetShadowQuality(shadowQuality);
    }

    // カスタム設定
    ImGui::Separator();
    ImGui::Text("Custom Settings");

    const char* sizeNames[]  = { "256", "512", "1024", "2048", "4096", "8192" };
    const int   sizeValues[] = { 256, 512, 1024, 2048, 4096, 8192 };
    // 解像度の欄。今の値を選択肢から探し、選び直されたら apply へ渡す
    const auto sizeCombo = [&](const char* label, uint32_t current, const auto& apply) {
        int index = 3; // デフォルトは2048
        for (int i = 0; i < IM_ARRAYSIZE(sizeValues); i++) {
            if (sizeValues[i] == static_cast<int>(current)) {
                index = i;
                break;
            }
        }
        if (UI::Combo(label, &index, sizeNames, IM_ARRAYSIZE(sizeNames))) {
            apply(static_cast<uint32_t>(sizeValues[index]));
        }
    };

    if (shadowMap_) {
        sizeCombo("Shadow Map Size", shadowMap_->GetShadowMapSize(), [&](uint32_t size) { SetShadowMapSize(size); });
        ImGui::SetItemTooltip("Directional light atlas. With 2 or more cascades, each cascade gets half of this size");
    }
    if (localShadowMap_) {
        sizeCombo("Local Shadow Map Size", localShadowMap_->GetShadowMapSize(), [&](uint32_t size) { SetLocalShadowMapSize(size); });
        ImGui::SetItemTooltip("Point / spot light atlas (4 x 4 = 16 slots). A spot light uses 1 slot, a point light uses 6");
    }

    if (shadowMap_) {
        // PCF カーネルサイズ
        int pcfKernelSize = shadowMap_->GetPCFKernelSize();
        const char* kernelNames[] = { "1x1 (No PCF)", "3x3", "5x5", "7x7" };
        int kernelValues[] = { 1, 3, 5, 7 };
        int currentKernelIndex = 1; // デフォルトは3x3
        for (int i = 0; i < IM_ARRAYSIZE(kernelValues); i++) {
            if (kernelValues[i] == pcfKernelSize) {
                currentKernelIndex = i;
                break;
            }
        }
        if (UI::Combo("PCF Kernel Size", &currentKernelIndex, kernelNames, IM_ARRAYSIZE(kernelNames))) {
            pcfKernelSize = kernelValues[currentKernelIndex];
            SetPCFKernelSize(pcfKernelSize);
        }
    }

    // カスケード
    ImGui::Separator();
    ImGui::Text("Cascades");
    int cascadeCount = cascadeCount_;
    if (UI::SliderInt("Cascade Count", &cascadeCount, 1, kMaxCascades)) {
        SetCascadeCount(cascadeCount);
    }
    UI::SliderFloat("Cascade Split", &cascadeSplit_, 0.0f, 1.0f, "%.2f");
    ImGui::SetItemTooltip("0 = evenly spaced, 1 = logarithmic (near cascades get smaller and sharper)");
    UI::Checkbox("Stagger Far Cascades", &staggerFarCascades_);
    ImGui::SetItemTooltip("Redraw the 3rd and 4th cascade every other frame.\nFewer shadow draws, but far shadows of moving objects lag by one frame");
    UI::Checkbox("Show Cascades", &showCascades_);
    ImGui::SetItemTooltip("Tint lit objects by cascade: red, green, blue, yellow (not saved)");

    // バイアス設定
    ImGui::Separator();
    ImGui::Text("Bias Settings");
    UI::DragFloat("Depth Bias", &depthBias_, 0.05f, 0.0f, 10.0f, "%.2f texel");
    ImGui::SetItemTooltip("Moves the lookup toward the light. Raise it when lit surfaces show stripes");
    UI::DragFloat("Normal Bias", &normalBias_, 0.05f, 0.0f, 10.0f, "%.2f texel");
    ImGui::SetItemTooltip("Moves the lookup along the surface normal. Raise it for stripes on slopes; too high detaches the shadow");

    // パフォーマンス情報
    ImGui::Separator();
    ImGui::Text("Performance Info");
    ImGui::Text("Shadow views this frame: %u (%u slots for point / spot lights)", static_cast<uint32_t>(views_.size()), kMaxLocalSlices);
    if (shadowMap_ && localShadowMap_) {
        const uint32_t mapSize   = shadowMap_->GetShadowMapSize();
        const uint32_t localSize = localShadowMap_->GetShadowMapSize();
        ImGui::Text("Cascade Size: %ux%u", cascadeCount_ > 1 ? mapSize / kCascadeGrid : mapSize, cascadeCount_ > 1 ? mapSize / kCascadeGrid : mapSize);
        ImGui::Text("Local Slot Size: %ux%u", localSize / kLocalGrid, localSize / kLocalGrid);

        // シャドウマップのメモリ使用量を表示（32bit depth）
        const float memoryMB = static_cast<float>(mapSize * mapSize + localSize * localSize) * 4.0f / (1024.0f * 1024.0f);
        ImGui::Text("Memory Usage: %.2f MB", memoryMB);

        // 深度をそのまま赤で出す（1 に近いほど明るい）
        if (ImGui::TreeNode("Shadow Maps")) {
            const float  width = (std::min)(ImGui::GetContentRegionAvail().x, 512.0f);
            const ImVec2 size(width, width);
            ImGui::TextDisabled("Directional (cascades)");
            ImGui::Image((ImTextureID)SrvManager::GetInstance()->GetGPUDescriptorHandle(shadowMap_->GetSrvIndex()).ptr, size);
            ImGui::TextDisabled("Point / Spot");
            ImGui::Image((ImTextureID)SrvManager::GetInstance()->GetGPUDescriptorHandle(localShadowMap_->GetSrvIndex()).ptr, size);
            ImGui::TreePop();
        }
    }

#endif // _DEBUG
}

} // namespace Tako
