#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <array>
#include "GPUParticleEmitter.h"
#include "EmitterStruct.h"
#include <json.hpp>

namespace Tako {

// 前方宣言
class GPUParticle;
class SphereEmitter;
class BoxEmitter;
class TriangleEmitter;

/// <summary>
/// エミッターグループ情報構造体
/// 複数のエミッターをグループ化して一括制御するための情報
/// </summary>
struct EmitterGroup {
  std::string name;                        ///< グループ名
  std::vector<std::string> emitterNames;   ///< グループに属するエミッター名のリスト
  bool isActive;                           ///< グループのアクティブ状態
};

/// <summary>
/// GPUパーティクルエミッター管理クラス
/// 球体、箱型、三角形の各種エミッターを名前で管理し、動的な生成・更新・削除をサポート
/// グループ機能、プリセット保存/読み込み、コピー&ペースト機能を提供
/// JSON形式でのエミッター設定の永続化に対応
/// </summary>
class EmitterManager
{
public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  /// <param name="particleSystem">GPUパーティクルシステムへのポインタ</param>
  EmitterManager(GPUParticle* particleSystem);

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~EmitterManager();

  /// <summary>
  /// 球形エミッターを作成
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="radius">球の半径</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">射出頻度（秒）</param>
  void CreateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count, float frequency);

  /// <summary>
  /// ボックス型エミッターを作成
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="size">ボックスのサイズ</param>
  /// <param name="rotation">回転角度</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">射出頻度（秒）</param>
  void CreateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count, float frequency);

  /// <summary>
  /// 三角形エミッターを作成
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="v1">頂点1</param>
  /// <param name="v2">頂点2</param>
  /// <param name="v3">頂点3</param>
  /// <param name="count">パーティクル数</param>
  /// <param name="frequency">射出頻度（秒）</param>
  void CreateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count, float frequency);

  /// <summary>
  /// 球形エミッターのパラメータを更新
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="radius">球の半径</param>
  /// <param name="count">パーティクル数（0の場合は変更しない）</param>
  /// <param name="frequency">射出頻度（0の場合は変更しない）</param>
  void UpdateSphereEmitter(const std::string& name, const Vector3& position, float radius,
    uint32_t count = 0, float frequency = 0.0f);

  /// <summary>
  /// ボックス型エミッターのパラメータを更新
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="size">ボックスのサイズ</param>
  /// <param name="rotation">回転角度</param>
  /// <param name="count">パーティクル数（0の場合は変更しない）</param>
  /// <param name="frequency">射出頻度（0の場合は変更しない）</param>
  void UpdateBoxEmitter(const std::string& name, const Vector3& position, const Vector3& size,
    const Vector3& rotation, uint32_t count = 0, float frequency = 0.0f);

  /// <summary>
  /// 三角形エミッターのパラメータを更新
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  /// <param name="v1">頂点1</param>
  /// <param name="v2">頂点2</param>
  /// <param name="v3">頂点3</param>
  /// <param name="count">パーティクル数（0の場合は変更しない）</param>
  /// <param name="frequency">射出頻度（0の場合は変更しない）</param>
  void UpdateTriangleEmitter(const std::string& name, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count = 0, float frequency = 0.0f);


  /// <summary>
  /// 既存のエミッターから一時的なエミッターを作成
  /// </summary>
  /// <param name="sourceName">コピー元のエミッター名</param>
  /// <param name="newName">新しいエミッター名</param>
  /// <param name="lifeTime">一時エミッターの寿命（秒）</param>
  void CreateTemporaryEmitterFrom(const std::string& sourceName, const std::string& newName, float lifeTime);

  /// <summary>
  /// 更新処理（一時的なエミッターの寿命管理など）
  /// </summary>
  void Update();

  /// <summary>
  /// エミッターの位置を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="position">位置</param>
  void SetEmitterPosition(const std::string& name, const Vector3& position);

  /// <summary>
  /// エミッターのスケール範囲を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="scaleRangeX">X方向のスケール範囲</param>
  /// <param name="scaleRangeY">Y方向のスケール範囲</param>
  void SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY);

  /// <summary>
  /// エミッターの速度範囲を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="velRangeX">X方向の速度範囲</param>
  /// <param name="velRangeY">Y方向の速度範囲</param>
  /// <param name="velRangeZ">Z方向の速度範囲</param>
  void SetEmitterVelocityRange(const std::string& name, const Vector2& velRangeX, const Vector2& velRangeY, const Vector2& velRangeZ);

  /// <summary>
  /// エミッターのパーティクル寿命範囲を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="lifeTimeRange">寿命範囲（秒）</param>
  void SetEmitterLifeTimeRange(const std::string& name, const Vector2& lifeTimeRange);

  /// <summary>
  /// エミッターのアクティブ状態を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="isActive">アクティブにする場合true</param>
  void SetEmitterActive(const std::string& name, bool isActive);

  /// <summary>
  /// エミッターの発生数を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="count">パーティクル数</param>
  void SetEmitterCount(const std::string& name, const uint32_t count);

  /// <summary>
  /// エミッターの速度正規化を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="isNormalize">正規化する場合true</param>
  void SetEmitterNormalize(const std::string& name, bool isNormalize);

  /// <summary>
  /// エミッターのランダムZ軸回転を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="isRandomRotateZ">ランダム回転を有効にする場合true</param>
  void SetEmitterRandomRotateZ(const std::string& name, bool isRandomRotateZ);

  /// <summary>
  /// エミッターの色を設定（開始色と終了色を同じ値に）
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="color">色</param>
  void SetEmitterColor(const std::string& name, const Vector4& color);

  /// <summary>
  /// エミッターの開始色を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="color">開始色</param>
  void SetEmitterStartColor(const std::string& name, const Vector4& color);

  /// <summary>
  /// エミッターの終了色を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="color">終了色</param>
  void SetEmitterEndColor(const std::string& name, const Vector4& color);

  /// <summary>
  /// エミッターの開始色と終了色を設定
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="startColor">開始色</param>
  /// <param name="endColor">終了色</param>
  void SetEmitterColors(const std::string& name, const Vector4& startColor, const Vector4& endColor);

  /// <summary>
  /// エミッターの半径を設定（球形エミッター専用）
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <param name="radius">半径</param>
  void SetEmitterRadius(const std::string& name, float radius);

  /// <summary>
  /// 名前でエミッターを取得
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <returns>見つかったエミッター、見つからない場合はnullptr</returns>
  std::shared_ptr<GPUParticleEmitter> GetEmitterByName(const std::string& name);

  /// <summary>
  /// エミッターを削除
  /// </summary>
  /// <param name="name">エミッター名</param>
  void RemoveEmitter(const std::string& name);

  /// <summary>
  /// 全てのエミッターを削除
  /// </summary>
  void RemoveAllEmitters();

  /// <summary>
  /// エミッターグループを作成
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void CreateGroup(const std::string& groupName);

  /// <summary>
  /// グループにエミッターを追加
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="emitterName">エミッター名</param>
  void AddToGroup(const std::string& groupName, const std::string& emitterName);

  /// <summary>
  /// グループからエミッターを削除
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="emitterName">エミッター名</param>
  void RemoveFromGroup(const std::string& groupName, const std::string& emitterName);

  /// <summary>
  /// グループのアクティブ状態を設定
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="isActive">アクティブにする場合true</param>
  void SetGroupActive(const std::string& groupName, bool isActive);

  /// <summary>
  /// グループ内の全エミッターの位置を設定
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="position">位置</param>
  void SetGroupPosition(const std::string& groupName, const Vector3& position);

  /// <summary>
  /// グループを削除
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void RemoveGroup(const std::string& groupName);

  /// <summary>
  /// デバッグ情報を表示
  /// </summary>
  void DebugInfo();

  /// <summary>
  /// アクティブなエミッターの数を取得
  /// </summary>
  /// <returns>エミッター数</returns>
  size_t GetActiveEmitterCount() const { return emitterMap_.size(); }

  /// <summary>
  /// エミッター設定をJSONファイルに保存
  /// </summary>
  /// <param name="filename">ファイル名</param>
  void SaveScenePreset(const std::string& filename);

  /// <summary>
  /// JSONファイルからエミッター設定を読み込み
  /// </summary>
  /// <param name="filename">ファイル名</param>
  void LoadScenePreset(const std::string& filename);

  /// <summary>
  /// エミッター設定をプリセットとして保存
  /// </summary>
  /// <param name="presetName">プリセット名</param>
  /// <param name="emitterName">エミッター名</param>
  void SavePreset(const std::string& presetName, const std::string& emitterName);

  /// <summary>
  /// プリセットからエミッターを作成
  /// </summary>
  /// <param name="presetName">プリセット名</param>
  /// <param name="newEmitterName">新しいエミッター名</param>
  void LoadPreset(const std::string& presetName, const std::string& newEmitterName);

  /// <summary>
  /// プリセットからエミッターを作成,既存のエミッター名を使用
  /// </summary>
  /// <param name="presetName">プリセット名</param>
  void LoadPreset(const std::string& presetName);

  /// <summary>
  /// 全てのエミッター名を取得
  /// </summary>
  /// <returns>エミッター名のリスト</returns>
  std::vector<std::string> GetEmitterNames() const;

  /// <summary>
  /// 指定した名前のエミッターが存在するか確認
  /// </summary>
  /// <param name="name">エミッター名</param>
  /// <returns>存在する場合true</returns>
  bool HasEmitter(const std::string& name) const;

  /// <summary>
  /// エミッター設定をコピー
  /// </summary>
  /// <param name="emitterName">コピー元のエミッター名</param>
  /// <param name="slotIndex">コピー先スロット番号（0-4）</param>
  /// <returns>成功した場合true</returns>
  bool CopyEmitterSettings(const std::string& emitterName, int slotIndex = 0);

  /// <summary>
  /// エミッター設定をペースト
  /// </summary>
  /// <param name="targetEmitterName">ペースト先のエミッター名</param>
  /// <param name="slotIndex">コピー元スロット番号（0-4）</param>
  /// <param name="colorOnly">色のみペーストする場合true</param>
  /// <param name="velocityOnly">速度のみペーストする場合true</param>
  /// <param name="scaleOnly">スケールのみペーストする場合true</param>
  /// <returns>成功した場合true</returns>
  bool PasteEmitterSettings(const std::string& targetEmitterName, int slotIndex = 0, bool colorOnly = false, bool velocityOnly = false, bool scaleOnly = false);

  /// <summary>
  /// 指定したスロットにコピー済みの設定があるか確認
  /// </summary>
  /// <param name="slotIndex">スロット番号（0-4）</param>
  /// <returns>設定がある場合true</returns>
  bool HasCopiedSettings(int slotIndex = 0) const;

  /// <summary>
  /// 指定したスロットのコピー済み設定をクリア
  /// </summary>
  /// <param name="slotIndex">スロット番号（0-4）</param>
  void ClearCopiedSettings(int slotIndex = 0);

  /// <summary>
  /// 全てのグループ名を取得
  /// </summary>
  /// <returns>グループ名のリスト</returns>
  std::vector<std::string> GetGroupNames() const;

  /// <summary>
  /// 指定したグループに属するエミッター名を取得
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <returns>エミッター名のリスト</returns>
  std::vector<std::string> GetEmittersInGroup(const std::string& groupName) const;

  /// <summary>
  /// グループがアクティブかどうかを取得
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <returns>アクティブな場合true</returns>
  bool IsGroupActive(const std::string& groupName) const;

  /// <summary>
  /// グループの数を取得
  /// </summary>
  /// <returns>グループ数</returns>
  size_t GetGroupCount() const { return groupMap_.size(); }

private: // プライベートメンバー関数

  /// <summary>
  /// 一時的なエミッターの更新（Update関数内で呼び出される）
  /// </summary>
  void UpdateTemporaryEmitters();

  /// <summary>
  /// エミッターをJSONにシリアライズ
  /// </summary>
  /// <param name="emitter">シリアライズするエミッター</param>
  /// <param name="json">出力先のJSONオブジェクト</param>
  void SerializeEmitterToJSON(const std::shared_ptr<GPUParticleEmitter>& emitter, nlohmann::json& json) const;

  /// <summary>
  /// JSONからエミッターをデシリアライズ
  /// </summary>
  /// <param name="json">読み込むJSONオブジェクト</param>
  /// <returns>デシリアライズされたエミッター</returns>
  std::shared_ptr<GPUParticleEmitter> DeserializeEmitterFromJSON(const nlohmann::json& json);

private:
  /// <summary>
  /// GPUパーティクルシステムへのポインタ
  /// </summary>
  GPUParticle* particleSystem_;

  /// <summary>
  /// エミッター名からエミッターへのマップ（名前ベース管理）
  /// </summary>
  std::unordered_map<std::string, std::shared_ptr<GPUParticleEmitter>> emitterMap_;

  /// <summary>
  /// グループ名からグループ情報へのマップ
  /// </summary>
  std::unordered_map<std::string, EmitterGroup> groupMap_;

  /// <summary>
  /// コピーバッファスロット構造体
  /// エミッター設定のコピー&ペースト用の一時保存領域
  /// </summary>
  struct CopiedSettings {
    bool valid = false;   ///< このスロットが有効なデータを持っているか
    EmitterData data;     ///< コピーされたエミッターデータ
    EmitterType type;     ///< コピーされたエミッタータイプ
  };
  std::array<CopiedSettings, 5> copiedSettingsSlots_; ///< コピーバッファ（5スロット分）

};

} // namespace Tako