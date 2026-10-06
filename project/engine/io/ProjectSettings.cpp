#include "ProjectSettings.h"
#include "DX12Basic.h"
#include "ShadowRenderer.h"
#include "SceneManager.h"
#include "FrameTimer.h"
#include "PostEffectManager.h"
#include "CollisionManager.h"
#include "Input.h"
#include "Logger.h"

#include <json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <utility>

namespace Tako {

  namespace ProjectSettings {
    using json = nlohmann::json;

    namespace {
      constexpr Input::NamedCode<Input::Stick> kStickTable[] = {
        { Input::Stick::None, "None" }, { Input::Stick::Left, "Left" }, { Input::Stick::Right, "Right" },
      };

      constexpr Input::NamedCode<PostEffectManager::AntiAliasing> kAntiAliasingTable[] = {
        { PostEffectManager::AntiAliasing::None, "None" }, { PostEffectManager::AntiAliasing::FXAA, "FXAA" },
      };

      const json& Section(const json& root, const char* key) {
        static const json empty = json::object();
        return root.contains(key) ? root[key] : empty;
      }

      /// <summary>
      /// ファイル全体を読む。無ければ nullopt（例外は呼び出し側で捕まえる）
      /// </summary>
      std::optional<json> ReadRoot() {
        std::ifstream ifs(kFilePath);
        if (!ifs) {
          return std::nullopt;
        }
        return json::parse(ifs);
      }

      BYTE KeyCode(const json& binding, const char* key) {
        if (!binding.contains(key)) {
          return 0;
        }
        const std::string name = binding[key].get<std::string>();
        const auto code = Input::FindCode(Input::GetKeyTable(), name);
        if (!code) {
          Logger::Log("Project settings: unknown key \"%s\"", name.c_str());
        }
        return code.value_or(0);
      }

      void LoadInput(const json& input) {
        Input* inputSystem = Input::GetInstance();
        inputSystem->SetLeftStickDeadZone(input.value("LeftStickDeadZone", inputSystem->GetLeftStickDeadZone()));
        inputSystem->SetRightStickDeadZone(input.value("RightStickDeadZone", inputSystem->GetRightStickDeadZone()));

        GeneratedHeaders& headers = GetGeneratedHeaders();
        headers.inputActions = input.value("ActionNameHeader", headers.inputActions);
        headers.inputAxes    = input.value("AxisNameHeader", headers.inputAxes);

        if (input.contains("Actions")) {
          Input::ActionMap actions;
          for (const auto& item : input["Actions"].items()) {
            Input::ActionBinding& binding = actions[item.key()];
            for (const json& key : item.value().value("Keys", json::array())) {
              if (const auto code = Input::FindCode(Input::GetKeyTable(), key.get<std::string>())) {
                binding.keys.push_back(*code);
              }
            }
            for (const json& button : item.value().value("Buttons", json::array())) {
              if (const auto code = Input::FindCode(Input::GetButtonTable(), button.get<std::string>())) {
                binding.buttons.push_back(*code);
              }
            }
            for (const json& button : item.value().value("Mouse", json::array())) {
              if (const auto code = Input::FindCode(Input::GetMouseButtonTable(), button.get<std::string>())) {
                binding.mouseButtons.push_back(*code);
              }
            }
          }
          inputSystem->SetActions(std::move(actions));
        }

        if (input.contains("Axes")) {
          Input::AxisMap axes;
          for (const auto& item : input["Axes"].items()) {
            const json& node = item.value();
            const auto stick = Input::FindCode<Input::Stick>(kStickTable, node.value("Stick", std::string("None")));
            axes[item.key()] = Input::AxisBinding{
              .up    = KeyCode(node, "Up"),
              .down  = KeyCode(node, "Down"),
              .left  = KeyCode(node, "Left"),
              .right = KeyCode(node, "Right"),
              .stick = stick.value_or(Input::Stick::None),
            };
          }
          inputSystem->SetAxes(std::move(axes));
        }
      }

      json SaveInput() {
        Input* inputSystem = Input::GetInstance();

        // 割当は名前表から選んだコードしか入らないが、表に無いコードは書かずに落とす
        const auto appendName = [](json& list, const char* name) {
          if (name) {
            list.push_back(name);
          }
        };

        json actions = json::object();
        for (const auto& [name, binding] : inputSystem->GetActions()) {
          json keys    = json::array();
          json buttons = json::array();
          json mouse   = json::array();
          for (BYTE key : binding.keys) {
            appendName(keys, Input::FindName(Input::GetKeyTable(), key));
          }
          for (WORD button : binding.buttons) {
            appendName(buttons, Input::FindName(Input::GetButtonTable(), button));
          }
          for (int button : binding.mouseButtons) {
            appendName(mouse, Input::FindName(Input::GetMouseButtonTable(), button));
          }
          actions[name] = { { "Keys", keys }, { "Buttons", buttons }, { "Mouse", mouse } };
        }

        json axes = json::object();
        for (const auto& [name, axis] : inputSystem->GetAxes()) {
          json node = { { "Stick", Input::FindName<Input::Stick>(kStickTable, axis.stick) } };
          const std::pair<const char*, BYTE> directions[] = { { "Up", axis.up }, { "Down", axis.down }, { "Left", axis.left }, { "Right", axis.right } };
          for (const auto& [direction, code] : directions) {
            if (const char* keyName = Input::FindName(Input::GetKeyTable(), code)) {
              node[direction] = keyName;
            }
          }
          axes[name] = std::move(node);
        }

        return {
          { "LeftStickDeadZone",  inputSystem->GetLeftStickDeadZone() },
          { "RightStickDeadZone", inputSystem->GetRightStickDeadZone() },
          { "ActionNameHeader",   GetGeneratedHeaders().inputActions },
          { "AxisNameHeader",     GetGeneratedHeaders().inputAxes },
          { "Actions",            actions },
          { "Axes",               axes },
        };
      }

      void LoadPhysics(const json& physics) {
        CollisionManager* collision = CollisionManager::GetInstance();
        std::string& header = GetGeneratedHeaders().collisionLayers;
        header = physics.value("LayerEnumHeader", header);
        if (physics.contains("Layers")) {
          collision->SetLayerNames(physics["Layers"].get<std::vector<std::string>>());
        }
        // 層名が無いプロジェクトはマスクをコードで設定している前提なので触らない（Reload で消さないため）
        const std::vector<std::string>& names = collision->GetLayerNames();
        if (names.empty() || !physics.contains("CollisionPairs")) {
          return;
        }

        const auto layerId = [&names](const json& name) -> std::optional<uint32_t> {
          const auto it = std::ranges::find(names, name.get<std::string>());
          return it != names.end() ? std::optional<uint32_t>(static_cast<uint32_t>(it - names.begin())) : std::nullopt;
        };

        collision->ClearCollisionMasks();
        for (const json& pair : physics["CollisionPairs"]) {
          const auto a = layerId(pair.at(0));
          const auto b = layerId(pair.at(1));
          if (a && b) {
            collision->SetCollisionMask(*a, *b, true);
          }
          else {
            Logger::Log("Project settings: unknown collision layer in %s", pair.dump().c_str());
          }
        }
      }

      json SavePhysics() {
        CollisionManager* collision = CollisionManager::GetInstance();
        const std::vector<std::string>& names = collision->GetLayerNames();

        // 名前のない型 ID の組み合わせは保存できないので、層名の範囲だけ書き出す
        json pairs = json::array();
        for (uint32_t a = 0; a < names.size(); ++a) {
          for (uint32_t b = a; b < names.size(); ++b) {
            if (collision->CanCollide(a, b)) {
              pairs.push_back({ names[a], names[b] });
            }
          }
        }
        return { { "LayerEnumHeader", GetGeneratedHeaders().collisionLayers }, { "Layers", names }, { "CollisionPairs", pairs } };
      }
    }

    WindowSettings& GetWindowSettings() {
      static WindowSettings settings;
      return settings;
    }

    GeneratedHeaders& GetGeneratedHeaders() {
      static GeneratedHeaders headers;
      return headers;
    }

    bool LoadWindow() {
      // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
      try {
        const std::optional<json> root = ReadRoot();
        if (!root) {
          return false;
        }

        WindowSettings& window = GetWindowSettings();
        const json& application = Section(*root, "Application");
        window.productName = application.value("ProductName", window.productName);

        const json& display = Section(*root, "Display");
        window.width           = display.value("Width", window.width);
        window.height          = display.value("Height", window.height);
        window.startFullscreen = display.value("StartFullscreen", window.startFullscreen);
        window.resizable       = display.value("Resizable", window.resizable);
        return true;
      }
      catch (const json::exception& e) {
        Logger::Log("Project settings load failed: %s", e.what());
        return false;
      }
    }

    bool Load(DX12Basic* dx12) {
      // 手編集された JSON の型不一致などは例外になるため、そこまでの適用を残して中断する
      try {
        const std::optional<json> root = ReadRoot();
        if (!root) {
          return false;
        }

        SceneManager* sceneManager = SceneManager::GetInstance();
        const json& application = Section(*root, "Application");
        sceneManager->SetStartupScene(application.value("StartupScene", sceneManager->GetStartupScene()));

        const json& display = Section(*root, "Display");
        dx12->SetVSync(display.value("VSync", dx12->IsVSync()));
        dx12->SetTargetFPS(display.value("TargetFPS", dx12->GetTargetFPS()));

        FrameTimer* timer = FrameTimer::GetInstance();
        const json& time = Section(*root, "Time");
        timer->SetMaxDeltaTime(time.value("MaxDeltaTime", timer->GetMaxDeltaTime()));

        ShadowRenderer* shadow = ShadowRenderer::GetInstance();
        PostEffectManager* postEffect = PostEffectManager::GetInstance();
        const json& rendering = Section(*root, "Rendering");
        shadow->SetEnabled(rendering.value("ShadowEnabled", shadow->IsEnabled()));
        shadow->SetShadowMapSize(rendering.value("ShadowMapSize", shadow->GetShadowMapSize()));
        shadow->SetPCFKernelSize(rendering.value("PCFKernelSize", shadow->GetPCFKernelSize()));
        shadow->SetMaxShadowDistance(rendering.value("MaxShadowDistance", shadow->GetMaxShadowDistance()));
        shadow->SetShadowBias(rendering.value("ShadowBias", shadow->GetShadowBias()));
        if (rendering.contains("ClearColor")) {
          const auto color = rendering["ClearColor"].get<std::array<float, 4>>();
          postEffect->SetClearColor({ color[0], color[1], color[2], color[3] });
        }
        const auto antiAliasing = Input::FindCode<PostEffectManager::AntiAliasing>(kAntiAliasingTable, rendering.value("AntiAliasing", std::string("None")));
        postEffect->SetAntiAliasing(antiAliasing.value_or(PostEffectManager::AntiAliasing::None));
        dx12->SetRenderScale(rendering.value("RenderScale", dx12->GetRenderScale()));

        LoadPhysics(Section(*root, "Physics"));
        LoadInput(Section(*root, "Input"));
        return true;
      }
      catch (const json::exception& e) {
        Logger::Log("Project settings load failed: %s", e.what());
        return false;
      }
    }

    bool Save(DX12Basic* dx12) {
      const WindowSettings& window     = GetWindowSettings();
      ShadowRenderer*       shadow     = ShadowRenderer::GetInstance();
      PostEffectManager*    postEffect = PostEffectManager::GetInstance();
      const Vector4&        clearColor = postEffect->GetClearColor();

      json root;
      root["Application"] = {
        { "ProductName",  window.productName },
        { "StartupScene", SceneManager::GetInstance()->GetStartupScene() },
      };
      root["Display"] = {
        { "Width",           window.width },
        { "Height",          window.height },
        { "StartFullscreen", window.startFullscreen },
        { "Resizable",       window.resizable },
        { "VSync",           dx12->IsVSync() },
        { "TargetFPS",       dx12->GetTargetFPS() },
      };
      root["Time"] = {
        { "MaxDeltaTime", FrameTimer::GetInstance()->GetMaxDeltaTime() },
      };
      root["Rendering"] = {
        { "ShadowEnabled",     shadow->IsEnabled() },
        { "ShadowMapSize",     shadow->GetShadowMapSize() },
        { "PCFKernelSize",     shadow->GetPCFKernelSize() },
        { "MaxShadowDistance", shadow->GetMaxShadowDistance() },
        { "ShadowBias",        shadow->GetShadowBias() },
        { "ClearColor",        { clearColor.x, clearColor.y, clearColor.z, clearColor.w } },
        { "AntiAliasing",      Input::FindName<PostEffectManager::AntiAliasing>(kAntiAliasingTable, postEffect->GetAntiAliasing()) },
        { "RenderScale",       dx12->GetRenderScale() },
      };
      root["Physics"] = SavePhysics();
      root["Input"]   = SaveInput();

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
