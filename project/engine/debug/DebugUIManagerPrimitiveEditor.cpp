#include "DebugUIManager.h"
#include "PreviewViewport.h"
#include "Object3d.h"
#include "Object3dBasic.h"
#include "Camera.h"
#include "Model.h"
#include "PrimitiveBuilder.h"
#include "TextureManager.h"
#include "FrameTimer.h"
#include "ImGuiManager.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

namespace Tako {

  DebugUIManager::DebugUIManager(Token) {}
  DebugUIManager::~DebugUIManager() = default;

  namespace {

    const char* const kPresetDirectory = "resources/Json/PrimitivePresets/";

    /// <summary>
    /// TextureManager::LoadTexture と同じパス解決規則でファイルの存在を確認する
    /// </summary>
    bool TextureFileExists(const std::string& name)
    {
      const std::string path = name.starts_with("EngineResources/") ? name : "resources/Texture/" + name;
      return std::filesystem::exists(path);
    }

    // JSON の type フィールド用。enum PrimitiveType および Type コンボの並びと一致させること
    constexpr const char* kPrimitiveTypeNames[] = { "Cube", "Sphere", "Plane", "Ring", "Cylinder", "Torus" };

    /// <summary>
    /// uint32_t 用 DragInt ラッパ（Ctrl+クリック直接入力は DragInt の min/max を無視するため明示クランプ）
    /// </summary>
    bool DragUint(const char* label, uint32_t& value, int minValue, int maxValue)
    {
      int v = static_cast<int>(value);
      if (ImGui::DragInt(label, &v, 1.0f, minValue, maxValue)) {
        value = static_cast<uint32_t>(std::clamp(v, minValue, maxValue));
        return true;
      }
      return false;
    }

    /// <summary>
    /// float を C++ リテラル文字列へ変換（最短往復表現、整数値は .0 を補い末尾 f）
    /// </summary>
    std::string FloatLit(float v)
    {
      std::string s = std::format("{}", v);
      if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
        s += ".0";
      }
      return s + "f";
    }

    /// <summary>
    /// designated initializer 形式の PrimitiveBuilder 呼び出し文字列を組み立てる
    /// </summary>
    std::string BuildCreateCall(const char* funcName, const std::vector<std::string>& fields)
    {
      std::string result = std::format("PrimitiveBuilder::{}(", funcName);
      if (!fields.empty()) {
        result += "{ ";
        for (size_t i = 0; i < fields.size(); ++i) {
          if (i > 0) {
            result += ", ";
          }
          result += fields[i];
        }
        result += " }";
      }
      result += ")";
      return result;
    }

  } // anonymous namespace

  void DebugUIManager::UpdatePrimitiveEditor()
  {
    if (!windowVisibility_["PrimitiveEditor"]) {
      // エディタを閉じたらプレビューを破棄
      primPreviewObject_.reset();
      primFloorObject_.reset();
      return;
    }

    if (!primPreviewViewport_) {
      primPreviewViewport_ = std::make_unique<PreviewViewport>();
      primPreviewViewport_->Initialize(L"PrimitiveEditorPreview");
    }

    if (!primPreviewCamera_) {
      primPreviewCamera_ = std::make_unique<Camera>();
      primPreviewCamera_->SetAspect(primPreviewViewport_->GetAspect());
      primPreviewCameraPtr_ = primPreviewCamera_.get();
    }

    if (!primFloorObject_) {
      primFloorObject_ = std::make_unique<Object3d>();
      primFloorObject_->Initialize();
      primFloorObject_->SetCamera(&primPreviewCameraPtr_);
      primFloorObject_->SetModel(PrimitiveBuilder::CreatePlane({ .width = 10.0f, .height = 10.0f }));
      primFloorObject_->SetMaterialColor(Vector4(0.35f, 0.35f, 0.35f, 1.0f));
      primFloorObject_->SetTranslate(Vector3(0.0f, -1.0f, 0.0f));
    }

    if (!primPreviewObject_) {
      primPreviewObject_ = std::make_unique<Object3d>();
      primPreviewObject_->Initialize();
      primPreviewObject_->SetCamera(&primPreviewCameraPtr_);
      primParamsDirty_ = true;
    }

    // モデル再生成はコマンド未記録・GPU アイドルのここでのみ行う（ImGui ハンドラ内での差し替えは記録済みコマンドの参照先を壊す）
    if (primParamsDirty_) {
      RebuildPrimitivePreview();
      primParamsDirty_ = false;
    }

    if (primAutoRotate_) {
      primPreviewRotate_.y += primAutoRotateSpeed_ * FrameTimer::GetInstance()->GetDeltaTime();
    }
    primPreviewObject_->SetRotate(primPreviewRotate_);
    primPreviewObject_->SetScale(primPreviewScale_);
    ApplyPrimitivePreviewSettings();

    primOrbitCamera_.ApplyTo(*primPreviewCamera_);

    primFloorObject_->Update();
    primPreviewObject_->Update();
  }

  void DebugUIManager::FinalizePrimitiveEditor()
  {
    primPreviewObject_.reset();
    primFloorObject_.reset();
    primPreviewViewport_.reset();
    primPreviewCamera_.reset();
    primPreviewCameraPtr_ = nullptr;
  }

  void DebugUIManager::RebuildPrimitivePreview()
  {
    if (!primPreviewObject_) {
      return;
    }

    std::unique_ptr<Model> model;
    switch (selectedPrimitiveType_) {
    case PrimitiveType::Cube:     model = PrimitiveBuilder::CreateCube(primCubeParams_);         break;
    case PrimitiveType::Sphere:   model = PrimitiveBuilder::CreateSphere(primSphereParams_);     break;
    case PrimitiveType::Plane:    model = PrimitiveBuilder::CreatePlane(primPlaneParams_);       break;
    case PrimitiveType::Ring:     model = PrimitiveBuilder::CreateRing(primRingParams_);         break;
    case PrimitiveType::Cylinder: model = PrimitiveBuilder::CreateCylinder(primCylinderParams_); break;
    case PrimitiveType::Torus:    model = PrimitiveBuilder::CreateTorus(primTorusParams_);       break;
    }

    // 旧 Model は SetModel 内で破棄され、SRV も Mesh のデストラクタで返却される
    primPreviewObject_->SetModel(std::move(model));
  }

  void DebugUIManager::ApplyPrimitivePreviewSettings()
  {
    if (!primPreviewObject_) {
      return;
    }
    // マテリアル系は Mesh 内データのため Model 差し替えで消える。毎フレーム再適用する
    primPreviewObject_->SetMaterialColor(primPreviewColor_);
    primPreviewObject_->SetEnableLighting(primPreviewLighting_);
    primPreviewObject_->SetTransparent(primPreviewTransparent_);
    primPreviewObject_->SetShininess(primMaterialShininess_);
    primPreviewObject_->SetEnableHighlight(primMaterialHighlight_);
    primPreviewObject_->SetUvTransform(Transform{
      { primMaterialUvScale_.x, primMaterialUvScale_.y, 1.0f },
      { 0.0f, 0.0f, primMaterialUvRotate_ },
      { primMaterialUvOffset_.x, primMaterialUvOffset_.y, 0.0f } });
    primPreviewObject_->SetTexture(primMaterialTexture_);  // 空/同一パスは Mesh 側で早期 return
  }

  void DebugUIManager::DrawPrimitivePreviewPass()
  {
    if (!windowVisibility_["PrimitiveEditor"] || !primPreviewViewport_ || !primPreviewObject_) {
      return;
    }

    primPreviewViewport_->BeginPass();

    // 直前は別パスの PSO のため共通描画設定を再適用（カメラ非依存なのでそのまま使える）
    Object3dBasic::GetInstance()->SetCommonRenderSetting();

    if (primShowFloor_ && primFloorObject_) {
      primFloorObject_->Draw();
    }
    primPreviewObject_->Draw();

    primPreviewViewport_->EndPass();
  }

  void DebugUIManager::DrawPrimitiveEditor()
  {
    ImGui::SetNextWindowSize(ImVec2(1000.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Primitive Editor", &windowVisibility_["PrimitiveEditor"])) {

      //---------------- 左: プレビュービューポート ----------------//
      // 右端ドラッグで幅調整可。サイズ指定は初期値で、調整結果は imgui.ini に保存される
      ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
      if (ImGui::BeginChild("Viewport##PrimEditor", ImVec2(620.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        if (ImGui::Button("Reset Camera##PrimEditor")) {
          primOrbitCamera_.Reset();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("RMB: Orbit / MMB: Pan / Wheel: Zoom");

        if (primPreviewViewport_ && primPreviewViewport_->IsInitialized()) {
          // ビューポート上のマウス操作でオービットカメラを制御（ImGui 経由なので Input クラスやテキスト入力と干渉しない）
          if (primPreviewViewport_->DrawImGuiImage()) {
            primOrbitCamera_.HandleImGuiInput();
          }
        }
        else {
          ImGui::TextDisabled("Initializing preview...");
        }
      }
      ImGui::EndChild();

      //---------------- 右: パラメータパネル ----------------//
      ImGui::SameLine();
      if (ImGui::BeginChild("Params##PrimEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {

        int typeIndex = static_cast<int>(selectedPrimitiveType_);
        if (ImGui::Combo("Type##PrimEditor", &typeIndex, "Cube\0Sphere\0Plane\0Ring\0Cylinder\0Torus\0")) {
          selectedPrimitiveType_ = static_cast<PrimitiveType>(typeIndex);
          primParamsDirty_ = true;
        }

        if (ImGui::CollapsingHeader("Shape Parameters##PrimEditor", ImGuiTreeNodeFlags_DefaultOpen)) {
          bool changed = false;
          switch (selectedPrimitiveType_) {
          case PrimitiveType::Cube:
            changed |= ImGui::DragFloat("Size##PrimCube", &primCubeParams_.size, 0.01f, 0.0f, 1000.0f);
            break;
          case PrimitiveType::Sphere:
            changed |= ImGui::DragFloat("Radius##PrimSphere", &primSphereParams_.radius, 0.01f, 0.0f, 1000.0f);
            changed |= DragUint("Lon Div##PrimSphere", primSphereParams_.lonDiv, 3, 256);
            changed |= DragUint("Lat Div##PrimSphere", primSphereParams_.latDiv, 2, 256);
            break;
          case PrimitiveType::Plane:
            changed |= ImGui::DragFloat("Width##PrimPlane", &primPlaneParams_.width, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Height##PrimPlane", &primPlaneParams_.height, 0.01f, 0.0f, 1000.0f);
            changed |= DragUint("X Seg##PrimPlane", primPlaneParams_.xSeg, 1, 256);
            changed |= DragUint("Y Seg##PrimPlane", primPlaneParams_.ySeg, 1, 256);
            break;
          case PrimitiveType::Ring:
            changed |= ImGui::DragFloat("Inner Radius##PrimRing", &primRingParams_.innerRadius, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Outer Radius##PrimRing", &primRingParams_.outerRadius, 0.01f, 0.0f, 1000.0f);
            changed |= DragUint("Segments##PrimRing", primRingParams_.segments, 3, 512);
            changed |= DragUint("Ring Seg##PrimRing", primRingParams_.ringSeg, 1, 256);
            changed |= ImGui::DragFloat("Start Angle##PrimRing", &primRingParams_.startAngleDeg, 1.0f, -720.0f, 720.0f);
            changed |= ImGui::DragFloat("Sweep Angle##PrimRing", &primRingParams_.sweepAngleDeg, 1.0f, -720.0f, 720.0f);
            break;
          case PrimitiveType::Cylinder:
            changed |= ImGui::DragFloat("Top Radius##PrimCylinder", &primCylinderParams_.topRadius, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Bottom Radius##PrimCylinder", &primCylinderParams_.bottomRadius, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Height##PrimCylinder", &primCylinderParams_.height, 0.01f, 0.0f, 1000.0f);
            changed |= DragUint("Radial Div##PrimCylinder", primCylinderParams_.radialDiv, 3, 256);
            changed |= DragUint("Height Div##PrimCylinder", primCylinderParams_.heightDiv, 1, 256);
            changed |= ImGui::Checkbox("Cap Top##PrimCylinder", &primCylinderParams_.capTop);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("Cap Bottom##PrimCylinder", &primCylinderParams_.capBottom);
            break;
          case PrimitiveType::Torus:
            changed |= ImGui::DragFloat("Major Radius##PrimTorus", &primTorusParams_.majorRadius, 0.01f, 0.0f, 1000.0f);
            changed |= ImGui::DragFloat("Minor Radius##PrimTorus", &primTorusParams_.minorRadius, 0.01f, 0.0f, 1000.0f);
            changed |= DragUint("Major Div##PrimTorus", primTorusParams_.majorDiv, 3, 256);
            changed |= DragUint("Minor Div##PrimTorus", primTorusParams_.minorDiv, 3, 256);
            break;
          }
          if (changed) {
            primParamsDirty_ = true;
          }
        }

        if (ImGui::CollapsingHeader("Display##PrimEditor", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::DragFloat3("Rotation##PrimEditor", &primPreviewRotate_.x, 0.01f);
          ImGui::DragFloat3("Scale##PrimEditor", &primPreviewScale_.x, 0.01f, 0.0f, 100.0f);
          ImGui::Checkbox("Auto Rotate##PrimEditor", &primAutoRotate_);
          if (primAutoRotate_) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragFloat("Speed##PrimEditor", &primAutoRotateSpeed_, 0.01f, -10.0f, 10.0f);
          }
          ImGui::Checkbox("Show Floor##PrimEditor", &primShowFloor_);
        }

        if (ImGui::CollapsingHeader("Material##PrimEditor", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::ColorEdit4("Color##PrimEditor", &primPreviewColor_.x);
          ImGui::Checkbox("Lighting##PrimEditor", &primPreviewLighting_);
          ImGui::SameLine();
          ImGui::Checkbox("Transparent##PrimEditor", &primPreviewTransparent_);
          ImGui::Checkbox("Highlight##PrimEditor", &primMaterialHighlight_);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(100.0f);
          ImGui::DragFloat("Shininess##PrimEditor", &primMaterialShininess_, 1.0f, 1.0f, 1000.0f);

          // ロード済みテクスチャから選択。手入力パスも入るためプレビューには現在値を表示
          if (ImGui::BeginCombo("Texture##PrimEditor", primMaterialTexture_.empty() ? "(default: white.dds)" : primMaterialTexture_.c_str())) {
            for (const std::string& texName : TextureManager::GetInstance()->GetLoadedTextureFileNames()) {
              const bool selected = (texName == primMaterialTexture_);
              if (ImGui::Selectable(texName.c_str(), selected)) {
                primMaterialTexture_ = texName;
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          static char texPathBuffer[256] = "";
          ImGui::SetNextItemWidth(150.0f);
          ImGui::InputText("##PrimTexPath", texPathBuffer, sizeof(texPathBuffer));
          ImGui::SameLine();
          if (ImGui::Button("Load & Set##PrimTex") && texPathBuffer[0] != '\0') {
            // LoadTexture は失敗時 assert のため事前に存在チェック
            if (TextureFileExists(texPathBuffer)) {
              primMaterialTexture_ = texPathBuffer;
            }
            else {
              AddLog(std::string("Primitive Editor: texture not found '") + texPathBuffer + "'", LogType::Error);
            }
          }
          ImGui::SameLine();
          if (ImGui::Button("Clear##PrimTex")) {
            // SetTexture は空文字を無視するため、Model 再生成でデフォルト white.dds に戻す
            primMaterialTexture_.clear();
            primParamsDirty_ = true;
          }

          ImGui::DragFloat2("UV Tiling##PrimEditor", &primMaterialUvScale_.x, 0.01f);
          ImGui::DragFloat2("UV Offset##PrimEditor", &primMaterialUvOffset_.x, 0.01f);
          ImGui::DragFloat("UV Rotate##PrimEditor", &primMaterialUvRotate_, 0.01f);
        }

        if (ImGui::CollapsingHeader("Output##PrimEditor", ImGuiTreeNodeFlags_DefaultOpen)) {
          const std::string cppString = GeneratePrimitiveCppString();
          ImGui::TextWrapped("%s", cppString.c_str());
          if (ImGui::Button("Copy as C++##PrimEditor")) {
            ImGui::SetClipboardText(cppString.c_str());
            AddLog("Primitive Editor: copied C++ snippet to clipboard", LogType::Info);
          }

          ImGui::SeparatorText("Preset");
          ImGui::SetNextItemWidth(150.0f);
          ImGui::InputText("##PrimPresetName", primPresetNameBuffer_, IM_ARRAYSIZE(primPresetNameBuffer_));
          ImGui::SameLine();
          if (ImGui::Button("Save##PrimPreset") && primPresetNameBuffer_[0] != '\0') {
            if (SavePrimitivePreset(primPresetNameBuffer_)) {
              primSelectedPreset_ = primPresetNameBuffer_;
            }
          }

          ImGui::SetNextItemWidth(150.0f);
          if (ImGui::BeginCombo("##PrimPresetList", primSelectedPreset_.empty() ? "(select preset)" : primSelectedPreset_.c_str())) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(kPresetDirectory, ec)) {
              if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
              }
              const std::string name = entry.path().stem().string();
              const bool selected = (name == primSelectedPreset_);
              if (ImGui::Selectable(name.c_str(), selected)) {
                primSelectedPreset_ = name;
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          ImGui::SameLine();
          if (ImGui::Button("Load##PrimPreset") && !primSelectedPreset_.empty()) {
            LoadPrimitivePreset(primSelectedPreset_);
          }

          ImGui::SeparatorText("Export OBJ");
          ImGui::SetNextItemWidth(150.0f);
          ImGui::InputText("##PrimExportName", primExportNameBuffer_, IM_ARRAYSIZE(primExportNameBuffer_));
          ImGui::SameLine();
          if (ImGui::Button("Export##PrimObj") && primExportNameBuffer_[0] != '\0') {
            const Model* model = primPreviewObject_ ? primPreviewObject_->GetModel() : nullptr;
            if (model && PrimitiveBuilder::ExportObj(*model, primExportNameBuffer_)) {
              AddLog(std::string("Primitive Editor: exported 'resources/Model/") + primExportNameBuffer_ + ".obj'", LogType::Info);
            }
            else {
              AddLog("Primitive Editor: OBJ export failed", LogType::Error);
            }
          }
        }
      }
      ImGui::EndChild();
    }
    ImGui::End();
  }

  std::string DebugUIManager::GeneratePrimitiveCppString() const
  {
    std::vector<std::string> fields;

    switch (selectedPrimitiveType_) {
    case PrimitiveType::Cube: {
      const PrimitiveBuilder::CubeParams def{};
      const auto& p = primCubeParams_;
      if (p.size != def.size) fields.push_back(std::format(".size = {}", FloatLit(p.size)));
      return BuildCreateCall("CreateCube", fields);
    }
    case PrimitiveType::Sphere: {
      const PrimitiveBuilder::SphereParams def{};
      const auto& p = primSphereParams_;
      if (p.radius != def.radius) fields.push_back(std::format(".radius = {}", FloatLit(p.radius)));
      if (p.lonDiv != def.lonDiv) fields.push_back(std::format(".lonDiv = {}", p.lonDiv));
      if (p.latDiv != def.latDiv) fields.push_back(std::format(".latDiv = {}", p.latDiv));
      return BuildCreateCall("CreateSphere", fields);
    }
    case PrimitiveType::Plane: {
      const PrimitiveBuilder::PlaneParams def{};
      const auto& p = primPlaneParams_;
      if (p.width != def.width) fields.push_back(std::format(".width = {}", FloatLit(p.width)));
      if (p.height != def.height) fields.push_back(std::format(".height = {}", FloatLit(p.height)));
      if (p.xSeg != def.xSeg) fields.push_back(std::format(".xSeg = {}", p.xSeg));
      if (p.ySeg != def.ySeg) fields.push_back(std::format(".ySeg = {}", p.ySeg));
      return BuildCreateCall("CreatePlane", fields);
    }
    case PrimitiveType::Ring: {
      const PrimitiveBuilder::RingParams def{};
      const auto& p = primRingParams_;
      if (p.innerRadius != def.innerRadius) fields.push_back(std::format(".innerRadius = {}", FloatLit(p.innerRadius)));
      if (p.outerRadius != def.outerRadius) fields.push_back(std::format(".outerRadius = {}", FloatLit(p.outerRadius)));
      if (p.segments != def.segments) fields.push_back(std::format(".segments = {}", p.segments));
      if (p.ringSeg != def.ringSeg) fields.push_back(std::format(".ringSeg = {}", p.ringSeg));
      if (p.startAngleDeg != def.startAngleDeg) fields.push_back(std::format(".startAngleDeg = {}", FloatLit(p.startAngleDeg)));
      if (p.sweepAngleDeg != def.sweepAngleDeg) fields.push_back(std::format(".sweepAngleDeg = {}", FloatLit(p.sweepAngleDeg)));
      return BuildCreateCall("CreateRing", fields);
    }
    case PrimitiveType::Cylinder: {
      const PrimitiveBuilder::CylinderParams def{};
      const auto& p = primCylinderParams_;
      if (p.topRadius != def.topRadius) fields.push_back(std::format(".topRadius = {}", FloatLit(p.topRadius)));
      if (p.bottomRadius != def.bottomRadius) fields.push_back(std::format(".bottomRadius = {}", FloatLit(p.bottomRadius)));
      if (p.height != def.height) fields.push_back(std::format(".height = {}", FloatLit(p.height)));
      if (p.radialDiv != def.radialDiv) fields.push_back(std::format(".radialDiv = {}", p.radialDiv));
      if (p.heightDiv != def.heightDiv) fields.push_back(std::format(".heightDiv = {}", p.heightDiv));
      if (p.capTop != def.capTop) fields.push_back(std::format(".capTop = {}", p.capTop ? "true" : "false"));
      if (p.capBottom != def.capBottom) fields.push_back(std::format(".capBottom = {}", p.capBottom ? "true" : "false"));
      return BuildCreateCall("CreateCylinder", fields);
    }
    case PrimitiveType::Torus: {
      const PrimitiveBuilder::TorusParams def{};
      const auto& p = primTorusParams_;
      if (p.majorRadius != def.majorRadius) fields.push_back(std::format(".majorRadius = {}", FloatLit(p.majorRadius)));
      if (p.minorRadius != def.minorRadius) fields.push_back(std::format(".minorRadius = {}", FloatLit(p.minorRadius)));
      if (p.majorDiv != def.majorDiv) fields.push_back(std::format(".majorDiv = {}", p.majorDiv));
      if (p.minorDiv != def.minorDiv) fields.push_back(std::format(".minorDiv = {}", p.minorDiv));
      return BuildCreateCall("CreateTorus", fields);
    }
    }
    return "";
  }

  bool DebugUIManager::SavePrimitivePreset(const std::string& name)
  {
    using json = nlohmann::json;

    json preset;
    preset["type"] = kPrimitiveTypeNames[static_cast<int>(selectedPrimitiveType_)];
    json& params = preset["params"];

    switch (selectedPrimitiveType_) {
    case PrimitiveType::Cube:
      params["size"] = primCubeParams_.size;
      break;
    case PrimitiveType::Sphere:
      params["radius"] = primSphereParams_.radius;
      params["lonDiv"] = primSphereParams_.lonDiv;
      params["latDiv"] = primSphereParams_.latDiv;
      break;
    case PrimitiveType::Plane:
      params["width"] = primPlaneParams_.width;
      params["height"] = primPlaneParams_.height;
      params["xSeg"] = primPlaneParams_.xSeg;
      params["ySeg"] = primPlaneParams_.ySeg;
      break;
    case PrimitiveType::Ring:
      params["innerRadius"] = primRingParams_.innerRadius;
      params["outerRadius"] = primRingParams_.outerRadius;
      params["segments"] = primRingParams_.segments;
      params["ringSeg"] = primRingParams_.ringSeg;
      params["startAngleDeg"] = primRingParams_.startAngleDeg;
      params["sweepAngleDeg"] = primRingParams_.sweepAngleDeg;
      break;
    case PrimitiveType::Cylinder:
      params["topRadius"] = primCylinderParams_.topRadius;
      params["bottomRadius"] = primCylinderParams_.bottomRadius;
      params["height"] = primCylinderParams_.height;
      params["radialDiv"] = primCylinderParams_.radialDiv;
      params["heightDiv"] = primCylinderParams_.heightDiv;
      params["capTop"] = primCylinderParams_.capTop;
      params["capBottom"] = primCylinderParams_.capBottom;
      break;
    case PrimitiveType::Torus:
      params["majorRadius"] = primTorusParams_.majorRadius;
      params["minorRadius"] = primTorusParams_.minorRadius;
      params["majorDiv"] = primTorusParams_.majorDiv;
      params["minorDiv"] = primTorusParams_.minorDiv;
      break;
    }

    json& mat = preset["material"];
    mat["color"] = { primPreviewColor_.x, primPreviewColor_.y, primPreviewColor_.z, primPreviewColor_.w };
    mat["lighting"] = primPreviewLighting_;
    mat["transparent"] = primPreviewTransparent_;
    mat["highlight"] = primMaterialHighlight_;
    mat["shininess"] = primMaterialShininess_;
    mat["texture"] = primMaterialTexture_;
    mat["uvScale"] = { primMaterialUvScale_.x, primMaterialUvScale_.y };
    mat["uvOffset"] = { primMaterialUvOffset_.x, primMaterialUvOffset_.y };
    mat["uvRotate"] = primMaterialUvRotate_;

    if (!std::filesystem::exists(kPresetDirectory)) {
      std::filesystem::create_directories(kPresetDirectory);
    }

    std::ofstream ofs(std::string(kPresetDirectory) + name + ".json");
    if (!ofs.is_open()) {
      AddLog("Primitive Editor: failed to save preset '" + name + "'", LogType::Error);
      return false;
    }
    ofs << std::setw(2) << preset << std::endl;

    AddLog("Primitive Editor: saved preset '" + name + "'", LogType::Info);
    return true;
  }

  bool DebugUIManager::LoadPrimitivePreset(const std::string& name)
  {
    using json = nlohmann::json;

    std::ifstream ifs(std::string(kPresetDirectory) + name + ".json");
    if (!ifs.is_open()) {
      AddLog("Primitive Editor: preset '" + name + "' not found", LogType::Error);
      return false;
    }

    try {
      json preset;
      ifs >> preset;

      const std::string typeName = preset.value("type", "");
      int typeIndex = -1;
      for (int i = 0; i < static_cast<int>(std::size(kPrimitiveTypeNames)); ++i) {
        if (typeName == kPrimitiveTypeNames[i]) {
          typeIndex = i;
          break;
        }
      }
      if (typeIndex < 0) {
        AddLog("Primitive Editor: unknown primitive type '" + typeName + "'", LogType::Error);
        return false;
      }
      selectedPrimitiveType_ = static_cast<PrimitiveType>(typeIndex);

      // 欠損キーはデフォルト値で埋める（古いプリセットとの互換のため）
      const json params = preset.value("params", json::object());
      switch (selectedPrimitiveType_) {
      case PrimitiveType::Cube: {
        const PrimitiveBuilder::CubeParams def{};
        primCubeParams_.size = params.value("size", def.size);
        break;
      }
      case PrimitiveType::Sphere: {
        const PrimitiveBuilder::SphereParams def{};
        primSphereParams_.radius = params.value("radius", def.radius);
        primSphereParams_.lonDiv = params.value("lonDiv", def.lonDiv);
        primSphereParams_.latDiv = params.value("latDiv", def.latDiv);
        break;
      }
      case PrimitiveType::Plane: {
        const PrimitiveBuilder::PlaneParams def{};
        primPlaneParams_.width = params.value("width", def.width);
        primPlaneParams_.height = params.value("height", def.height);
        primPlaneParams_.xSeg = params.value("xSeg", def.xSeg);
        primPlaneParams_.ySeg = params.value("ySeg", def.ySeg);
        break;
      }
      case PrimitiveType::Ring: {
        const PrimitiveBuilder::RingParams def{};
        primRingParams_.innerRadius = params.value("innerRadius", def.innerRadius);
        primRingParams_.outerRadius = params.value("outerRadius", def.outerRadius);
        primRingParams_.segments = params.value("segments", def.segments);
        primRingParams_.ringSeg = params.value("ringSeg", def.ringSeg);
        primRingParams_.startAngleDeg = params.value("startAngleDeg", def.startAngleDeg);
        primRingParams_.sweepAngleDeg = params.value("sweepAngleDeg", def.sweepAngleDeg);
        break;
      }
      case PrimitiveType::Cylinder: {
        const PrimitiveBuilder::CylinderParams def{};
        primCylinderParams_.topRadius = params.value("topRadius", def.topRadius);
        primCylinderParams_.bottomRadius = params.value("bottomRadius", def.bottomRadius);
        primCylinderParams_.height = params.value("height", def.height);
        primCylinderParams_.radialDiv = params.value("radialDiv", def.radialDiv);
        primCylinderParams_.heightDiv = params.value("heightDiv", def.heightDiv);
        primCylinderParams_.capTop = params.value("capTop", def.capTop);
        primCylinderParams_.capBottom = params.value("capBottom", def.capBottom);
        break;
      }
      case PrimitiveType::Torus: {
        const PrimitiveBuilder::TorusParams def{};
        primTorusParams_.majorRadius = params.value("majorRadius", def.majorRadius);
        primTorusParams_.minorRadius = params.value("minorRadius", def.minorRadius);
        primTorusParams_.majorDiv = params.value("majorDiv", def.majorDiv);
        primTorusParams_.minorDiv = params.value("minorDiv", def.minorDiv);
        break;
      }
      }

      // material キーの無い旧プリセットは全項目デフォルトに戻す
      const json mat = preset.value("material", json::object());
      const json colorArr = mat.value("color", json::array({ 1.0f, 1.0f, 1.0f, 1.0f }));
      if (colorArr.is_array() && colorArr.size() == 4) {
        primPreviewColor_ = { colorArr[0].get<float>(), colorArr[1].get<float>(), colorArr[2].get<float>(), colorArr[3].get<float>() };
      }
      primPreviewLighting_ = mat.value("lighting", true);
      primPreviewTransparent_ = mat.value("transparent", false);
      primMaterialHighlight_ = mat.value("highlight", true);
      primMaterialShininess_ = mat.value("shininess", 15.0f);
      primMaterialTexture_ = mat.value("texture", std::string());
      const json uvScaleArr = mat.value("uvScale", json::array({ 1.0f, 1.0f }));
      if (uvScaleArr.is_array() && uvScaleArr.size() == 2) {
        primMaterialUvScale_ = { uvScaleArr[0].get<float>(), uvScaleArr[1].get<float>() };
      }
      const json uvOffsetArr = mat.value("uvOffset", json::array({ 0.0f, 0.0f }));
      if (uvOffsetArr.is_array() && uvOffsetArr.size() == 2) {
        primMaterialUvOffset_ = { uvOffsetArr[0].get<float>(), uvOffsetArr[1].get<float>() };
      }
      primMaterialUvRotate_ = mat.value("uvRotate", 0.0f);

      // 消えたテクスチャは Apply 時の LoadTexture が assert で落ちるため空にフォールバック
      if (!primMaterialTexture_.empty() && !TextureFileExists(primMaterialTexture_)) {
        AddLog("Primitive Editor: preset texture not found '" + primMaterialTexture_ + "'", LogType::Warning);
        primMaterialTexture_.clear();
      }
    }
    catch (const json::exception&) {
      AddLog("Primitive Editor: failed to parse preset '" + name + "'", LogType::Error);
      return false;
    }

    primParamsDirty_ = true;
    AddLog("Primitive Editor: loaded preset '" + name + "'", LogType::Info);
    return true;
  }

} // namespace Tako
