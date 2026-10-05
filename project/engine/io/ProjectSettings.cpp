#include "ProjectSettings.h"
#include "DX12Basic.h"
#include "ShadowRenderer.h"
#include "Logger.h"

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace Tako {

  namespace ProjectSettings {
    using json = nlohmann::json;

    bool Load(DX12Basic* dx12) {
      std::ifstream ifs(kFilePath);
      if (!ifs) {
        return false;
      }

      // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
      try {
        const json root  = json::parse(ifs);
        const json empty = json::object();

        const json& display = root.contains("Display") ? root["Display"] : empty;
        dx12->SetVSync(display.value("VSync", dx12->IsVSync()));
        dx12->SetTargetFPS(display.value("TargetFPS", dx12->GetTargetFPS()));

        ShadowRenderer* shadow = ShadowRenderer::GetInstance();
        const json& rendering = root.contains("Rendering") ? root["Rendering"] : empty;
        shadow->SetEnabled(rendering.value("ShadowEnabled", shadow->IsEnabled()));
        shadow->SetShadowMapSize(rendering.value("ShadowMapSize", shadow->GetShadowMapSize()));
        shadow->SetPCFKernelSize(rendering.value("PCFKernelSize", shadow->GetPCFKernelSize()));
        shadow->SetMaxShadowDistance(rendering.value("MaxShadowDistance", shadow->GetMaxShadowDistance()));
        shadow->SetShadowBias(rendering.value("ShadowBias", shadow->GetShadowBias()));
        return true;
      }
      catch (const json::exception& e) {
        Logger::Log("Project settings load failed: %s", e.what());
        return false;
      }
    }

    bool Save(DX12Basic* dx12) {
      ShadowRenderer* shadow = ShadowRenderer::GetInstance();

      json root;
      root["Display"] = {
        { "VSync",     dx12->IsVSync() },
        { "TargetFPS", dx12->GetTargetFPS() },
      };
      root["Rendering"] = {
        { "ShadowEnabled",     shadow->IsEnabled() },
        { "ShadowMapSize",     shadow->GetShadowMapSize() },
        { "PCFKernelSize",     shadow->GetPCFKernelSize() },
        { "MaxShadowDistance", shadow->GetMaxShadowDistance() },
        { "ShadowBias",        shadow->GetShadowBias() },
      };

      std::error_code ec;
      std::filesystem::create_directories(std::filesystem::path(kFilePath).parent_path(), ec);
      std::ofstream ofs(kFilePath);
      if (!ofs) {
        return false;
      }
      ofs << std::setw(4) << root << std::endl;
      return true;
    }
  }

} // namespace Tako
