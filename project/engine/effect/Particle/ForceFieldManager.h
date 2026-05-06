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
  /// シーン全体への統合保存連携）を提供する。EmitterManager と対称な設計。
  ///
  /// JSON 仕様:
  ///  - type は 0-4 の数値（ForceFieldType）。enum 順序は変更しないこと。
  ///  - Vector3 は [x, y, z] 配列。
  ///  - GPU アライメント用 pad は JSON 対象外。
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

    //========================================
    // ファイル単位 API（Emitter と対称）
    //========================================

    /// <summary>
    /// 全フォースフィールドを JSON ファイルに保存（独立シーン保存）
    /// </summary>
    /// <param name="filename">拡張子なしのファイル名</param>
    void SaveScenePreset(const std::string& filename);

    /// <summary>
    /// JSON ファイルから全フォースフィールドを読み込み（既存リスト末尾に追加）
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

    //========================================
    // JSON object 単位 API（シーン統合保存連携用）
    //========================================

    /// <summary>
    /// 全フォースフィールドを root JSON の "forceFields" 配列に書き込む
    /// EmitterManager::SaveScenePreset から呼ばれる
    /// </summary>
    /// <param name="root">書き込み先 JSON（"forceFields" キーが追加される）</param>
    void SerializeAllToJSON(nlohmann::json& root) const;

    /// <summary>
    /// root JSON の "forceFields" 配列から全フォースフィールドを復元（リスト末尾に追加）
    /// EmitterManager::LoadScenePreset から呼ばれる
    /// 後方互換: "forceFields" キー不在 / 配列でない場合は何もしない
    /// </summary>
    /// <param name="root">読み込み元 JSON</param>
    void DeserializeAllFromJSON(const nlohmann::json& root);

  private:
    /// <summary>
    /// 単一フォースフィールドを JSON にシリアライズ（pad は除外）
    /// </summary>
    void SerializeForceFieldToJSON(const ForceFieldData& field, nlohmann::json& json) const;

    /// <summary>
    /// JSON から単一フォースフィールドをデシリアライズ
    /// </summary>
    /// <returns>必須キー（type, position）が揃っていれば true</returns>
    bool DeserializeForceFieldFromJSON(const nlohmann::json& json, ForceFieldData& outField) const;

  private:
    /// <summary>
    /// GPU パーティクルシステムへのポインタ（弱参照、所有しない）
    /// </summary>
    GPUParticle* particleSystem_ = nullptr;
  };

} // namespace Tako
