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
    /// 個別プリセットを読み込んで ForceFieldData に展開する（AddForceField はしない）。
    /// 呼び出し側で position 等を上書きしてから AddForceField したいケース用。
    /// </summary>
    /// <param name="presetName">プリセット名（拡張子なし）</param>
    /// <param name="outField">[out] 読み込み結果</param>
    /// <returns>読み込み成功で true / ファイル不在・JSON 不正で false</returns>
    bool LoadPresetToData(const std::string& presetName, ForceFieldData& outField) const;

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

    /// <summary>
    /// 指定位置に作用する力ベクトルの合計を CPU 側で評価する。
    /// GPU 側 compute shader と同等のロジックを CPU で再実装し、
    /// 弾やキャラクターなど CPU 駆動オブジェクトに同じ力場を作用させるためのクエリ API。
    /// </summary>
    /// <param name="pos">評価する世界座標</param>
    /// <param name="mask">評価対象とするビットフラグ。 (field.affectMask &amp; mask) != 0 の力場のみを合計</param>
    /// <returns>合計力ベクトル（加速度として velocity に加算する想定）</returns>
    /// <remarks>
    /// mask の意味解釈は呼び出し側の責任。エンジンはビット意味を一切定義しない。
    /// 既定値 0xFFFFFFFF を渡すと全力場が対象になる。
    /// </remarks>
    Vector3 EvaluateForceAt(const Vector3& pos, uint32_t mask = 0xFFFFFFFF) const;

    //=============================================================
    // GPUParticle への薄いラッパー（呼び出し側が GPUParticle を直接扱わなくて済むよう統一窓口を提供）
    //=============================================================

    /// <summary>
    /// フォースフィールドを追加（GPUParticle への薄いラッパー）
    /// </summary>
    /// <returns>登録インデックス（-1 = 失敗・上限到達）</returns>
    /// <remarks>
    /// RemoveForceField は erase ベースで以降のインデックスがシフトするため、
    /// 複数フィールドを削除する場合は **逆順インデックス** で RemoveForceField を呼ぶこと。
    /// </remarks>
    int32_t AddForceField(const ForceFieldData& field);

    /// <summary>
    /// 指定インデックスのフォースフィールドを更新（GPUParticle への薄いラッパー）
    /// </summary>
    void UpdateForceField(uint32_t index, const ForceFieldData& field);

    /// <summary>
    /// 指定インデックスのフォースフィールドを削除（GPUParticle への薄いラッパー）
    /// </summary>
    /// <remarks>
    /// 削除すると以降のインデックスは 1 ずつ前にシフトする。
    /// 連続削除する場合は必ず逆順（大きいインデックスから）で呼び出すこと。
    /// </remarks>
    void RemoveForceField(uint32_t index);

    /// <summary>
    /// 現在登録されているフォースフィールド数
    /// </summary>
    size_t GetForceFieldCount() const;

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
