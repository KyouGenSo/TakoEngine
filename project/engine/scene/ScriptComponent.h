#pragma once
#include "Component.h"

#include <json.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace Tako {

  class Script;

  /// <summary>
  /// C++ スクリプト（Script の派生）を 1 つ持ち、フックを渡すコンポーネント。
  /// 保存ではスクリプトの値を "script"（型名）と同じ階層に並べるので、スクリプトは "script" という名前のプロパティを持てない
  /// </summary>
  TCLASS()
  class ScriptComponent final : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    ScriptComponent();
    ~ScriptComponent() override;

    void BeginPlay() override;

    void Tick(float deltaTime) override;

    void EndPlay() override;

    /// <summary>
    /// 差し替えを予約されていれば、ここでスクリプトを作り直してから描画を渡す
    /// </summary>
    void Draw() override;

    void OnCollisionEnter(Actor* other) override;
    void OnCollisionStay(Actor* other) override;
    void OnCollisionExit(Actor* other) override;

    /// <summary>
    /// "script" の型でスクリプトを作り、同じ JSON からスクリプトの値を読む
    /// </summary>
    void PostLoad(const nlohmann::json& raw) override;

    /// <summary>
    /// スクリプトの値を out に並べる
    /// </summary>
    void PostSave(nlohmann::json& out) override;

#ifdef _DEBUG
    /// <summary>
    /// Script 欄（一覧から選ぶ・Assets からドロップする・Open で VS で開く）とスクリプトのプロパティ
    /// </summary>
    bool DrawCustomImGui() override;
#endif

    //=====================================
    //Setter
    //=====================================
    /// <summary>
    /// 次の Draw でスクリプトを作り直す（GPU が使っている資源を ImGui の中で消さないため）。同じキーの値は引き継ぐ
    /// </summary>
    /// <param name="typeName">Script の派生の型名。空なら外す</param>
    void SetScript(std::string_view typeName) { script_ = typeName; }

    //=====================================
    //Getter
    //=====================================
    Script* GetScript() const { return instance_.get(); }

    const std::string& GetScriptName() const { return script_; }

  private: //非公開関数
    /// <summary>
    /// script_ の型でなければ作り直し、values を読ませる。Play 中なら古い方の EndPlay と新しい方の BeginPlay を呼ぶ
    /// </summary>
    void RebuildInstance(const nlohmann::json& values);

  private: //メンバー変数
    TPROPERTY()
    std::string script_;

    std::unique_ptr<Script> instance_;
    std::string             builtScript_;            ///< instance_ を作ったときの script_（型が見つからなくても同じ名前で作り直さない）
    nlohmann::json          missingValues_;          ///< 型が見つからなかったときの値。保存で書き戻して失わない
    bool                    hasBegunPlay_  = false;
  };

} // namespace Tako
