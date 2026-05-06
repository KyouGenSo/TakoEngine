#pragma once
#include <string>
#include <vector>
#include "ParticleStruct.h"
#include <json.hpp>

namespace Tako {

  // 前方宣言
  class GPUParticle;

  /// <summary>
  /// フォースフィールド管理クラス
  /// GPUParticle が保持するフォースフィールドの JSON 永続化（個別プリセット保存/読込、
  /// シーン全体への統合保存）。
  /// </summary>
  class ForceFieldManager
  {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
    explicit ForceFieldManager(GPUParticle* particleSystem);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~ForceFieldManager() = default;

    /// <summary>
    /// 全フォースフィールドを ScenePreset JSON ファイルに保存
    /// </summary>
    /// <param name="filename">拡張子なしのファイル名</param>
    void SaveScenePreset(const std::string& filename);

    /// <summary>
    /// ScenePreset JSON ファイルから全フォースフィールドを読み込み
    /// </summary>
    /// <param name="filename">拡張子なしのファイル名</param>
    void LoadScenePreset(const std::string& filename);

    /// <summary>
    /// 指定インデックスのフォースフィールドを個別プリセットとして保存
    /// </summary>
    /// <param name="presetName">プリセット名（拡張子なし）</param>
    /// <param name="forceFieldIndex">保存対象のフォースフィールドインデックス</param>
    void SavePreset(const std::string& presetName, uint32_t forceFieldIndex);

    /// <summary>
    /// 個別プリセットからフォースフィールドを読み込み（リスト末尾に追加）
    /// </summary>
    /// <param name="presetName">プリセット名（拡張子なし）</param>
    void LoadPreset(const std::string& presetName);

    /// <summary>
    /// 全フォースフィールドを root JSON の "forceFields" 配列に書き込む
    /// EmitterManager::SaveScenePreset から呼ばれる
    /// </summary>
    /// <param name="root">書き込み先 JSON（"forceFields" キーが追加される）</param>
    void SerializeAllToJSON(nlohmann::json& root) const;

    /// <summary>
    /// root JSON の "forceFields" 配列から全フォースフィールドを復元
    /// </summary>
    /// <param name="root">読み込み元 JSON</param>
    void DeserializeAllFromJSON(const nlohmann::json& root);

  private:
    /// <summary>
    /// 単一フォースフィールドを JSON にシリアライズ
    /// </summary>
    void SerializeForceFieldToJSON(const ForceFieldData& field, nlohmann::json& json) const;

    /// <summary>
    /// JSON から単一フォースフィールドをデシリアライズ
    /// </summary>
    /// <returns>必須キー（type, position）が揃っていれば true</returns>
    bool DeserializeForceFieldFromJSON(const nlohmann::json& json, ForceFieldData& outField) const;

  private:

    GPUParticle* particleSystem_ = nullptr;
  };

} // namespace Tako
