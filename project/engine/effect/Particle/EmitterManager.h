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
  class MeshEmitter;
  class Object3d;
  class ForceFieldManager;

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
  /// GPU パーティクルエミッター管理クラス
  /// 球体、箱型、三角形の各種エミッターを名前で管理し、動的な生成・更新・削除をサポート
  /// グループ機能、プリセット保存/読み込み、コピー&ペースト機能を提供
  /// JSON 形式でのエミッター設定の永続化に対応
  /// </summary>
  class EmitterManager
  {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="particleSystem">GPU パーティクルシステムへのポインタ</param>
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
    /// Object3d を渡してメッシュエミッターを作成 (Object3d の world 行列に毎フレーム追従)
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <param name="obj3d">スポーン形状ソース兼追従先 (非所有、ライフタイム責務は呼び出し側)</param>
    /// <param name="count">パーティクル数</param>
    /// <param name="frequency">射出頻度 (秒)</param>

    void CreateMeshEmitter(const std::string& name, Object3d* obj3d, uint32_t count, float frequency);

    /// <summary>
    /// モデルファイルパスからメッシュエミッターを作成 (パーティクルエディタ用・JSON 永続化対応)。
    /// モデルは GPUParticle がロード・保持し、全メッシュをスポーン形状に使う。
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <param name="modelPath">モデルファイル名 (ModelManager 経由でロード)</param>
    /// <param name="count">パーティクル数</param>
    /// <param name="frequency">射出頻度 (秒)</param>
    void CreateMeshEmitterFromModel(const std::string& name, const std::string& modelPath, uint32_t count, float frequency);

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
    /// <param name="scaleRangeX">X 方向のスケール範囲</param>
    /// <param name="scaleRangeY">Y 方向のスケール範囲</param>
    void SetEmitterScaleRange(const std::string& name, const Vector2& scaleRangeX, const Vector2& scaleRangeY);

    /// <summary>
    /// エミッターの速度範囲を設定
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <param name="velRangeX">X 方向の速度範囲</param>
    /// <param name="velRangeY">Y 方向の速度範囲</param>
    /// <param name="velRangeZ">Z 方向の速度範囲</param>
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
    /// <param name="isActive">アクティブにする場合 true</param>
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
    /// <param name="isNormalize">正規化する場合 true</param>
    void SetEmitterNormalize(const std::string& name, bool isNormalize);

    /// <summary>
    /// エミッターのランダム Z 軸回転を設定
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <param name="isRandomRotateZ">ランダム回転を有効にする場合 true</param>
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
    /// <returns>見つかったエミッター、見つからない場合は nullptr</returns>
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
    /// <param name="isActive">アクティブにする場合 true</param>
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
    /// エミッター設定を JSON ファイルに保存
    /// </summary>
    /// <param name="filename">ファイル名</param>
    void SaveScenePreset(const std::string& filename);

    /// <summary>
    /// JSON ファイルからエミッター設定を読み込み
    /// </summary>
    /// <param name="filename">ファイル名</param>
    /// <remarks>
    /// Mesh エミッタは <c>meshModelPath</c> から自己完結で復元する。
    /// パスを持たない (Object3d バインド前提の) Mesh エミッタは警告ログを出してスキップ。
    /// </remarks>
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
    /// プリセットから Mesh エミッターを作成 (新しいエミッター名指定 + Object3d バインド版)
    /// </summary>
    /// <param name="presetName">プリセット名</param>
    /// <param name="newEmitterName">新しいエミッター名</param>
    /// <param name="obj3d">バインドする Object3d (スポーン形状もこのモデルから取得)</param>
    void LoadPreset(const std::string& presetName, const std::string& newEmitterName, Object3d* obj3d);

    /// <summary>
    /// プリセットから Mesh エミッターを作成 (既存名使用 + Object3d バインド版)
    /// </summary>
    /// <param name="presetName">プリセット名</param>
    /// <param name="obj3d">バインドする Object3d (スポーン形状もこのモデルから取得)</param>
    void LoadPreset(const std::string& presetName, Object3d* obj3d);

    /// <summary>
    /// 全てのエミッター名を取得
    /// </summary>
    /// <returns>エミッター名のリスト</returns>
    std::vector<std::string> GetEmitterNames() const;

    /// <summary>
    /// 指定した名前のエミッターが存在するか確認
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <returns>存在する場合 true</returns>
    bool HasEmitter(const std::string& name) const;

    /// <summary>
    /// エミッター設定をコピー
    /// </summary>
    /// <param name="emitterName">コピー元のエミッター名</param>
    /// <param name="slotIndex">コピー先スロット番号（0-4）</param>
    /// <returns>成功した場合 true</returns>
    bool CopyEmitterSettings(const std::string& emitterName, int slotIndex = 0);

    /// <summary>
    /// エミッター設定をペースト
    /// </summary>
    /// <param name="targetEmitterName">ペースト先のエミッター名</param>
    /// <param name="slotIndex">コピー元スロット番号（0-4）</param>
    /// <param name="colorOnly">色のみペーストする場合 true</param>
    /// <param name="velocityOnly">速度のみペーストする場合 true</param>
    /// <param name="scaleOnly">スケールのみペーストする場合 true</param>
    /// <returns>成功した場合 true</returns>
    bool PasteEmitterSettings(const std::string& targetEmitterName, int slotIndex = 0, bool colorOnly = false, bool velocityOnly = false, bool scaleOnly = false);

    /// <summary>
    /// 指定したスロットにコピー済みの設定があるか確認
    /// </summary>
    /// <param name="slotIndex">スロット番号（0-4）</param>
    /// <returns>設定がある場合 true</returns>
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
    /// <returns>アクティブな場合 true</returns>
    bool IsGroupActive(const std::string& groupName) const;

    /// <summary>
    /// グループの数を取得
    /// </summary>
    /// <returns>グループ数</returns>
    size_t GetGroupCount() const { return groupMap_.size(); }

    /// <summary>
    /// フォースフィールドマネージャを設定（弱参照、所有しない）
    /// 設定済みの場合、SaveScenePreset/LoadScenePreset でフォースフィールドも統合保存/読込される
    /// 未設定の場合はエミッタ・グループのみが対象（後方互換動作）
    /// </summary>
    /// <param name="manager">ForceFieldManager へのポインタ（nullptr で連携無効）</param>
    void SetForceFieldManager(ForceFieldManager* manager) { forceFieldManager_ = manager; }

  private: // プライベートメンバー関数

    /// <summary>
    /// 一時的なエミッターの更新（Update 関数内で呼び出される）
    /// </summary>
    void UpdateTemporaryEmitters();

    /// <summary>
    /// 生成済みエミッターを名前付きで登録する (同名は警告ログを出して置き換え)
    /// </summary>
    /// <param name="name">エミッター名</param>
    /// <param name="emitter">登録するエミッター</param>
    void AddNamedEmitter(const std::string& name, std::shared_ptr<GPUParticleEmitter> emitter);

    /// <summary>
    /// エミッターを JSON にシリアライズ
    /// </summary>
    /// <param name="emitter">シリアライズするエミッター</param>
    /// <param name="json">出力先の JSON オブジェクト</param>
    void SerializeEmitterToJSON(const std::shared_ptr<GPUParticleEmitter>& emitter, nlohmann::json& json) const;

    /// <summary>
    /// JSON からエミッターをデシリアライズ
    /// </summary>
    /// <param name="json">読み込む JSON オブジェクト</param>
    /// <param name="bindTarget">Mesh エミッタのバインド先 Object3d (nullptr で meshModelPath から自己完結復元)</param>
    /// <returns>デシリアライズされたエミッター (失敗時 nullptr)</returns>
    std::shared_ptr<GPUParticleEmitter> DeserializeEmitterFromJSON(
      const nlohmann::json& json, Object3d* bindTarget = nullptr);

  private:
    /// <summary>
    /// GPU パーティクルシステムへのポインタ
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
      bool valid = false;          ///< このスロットが有効なデータを持っているか
      EmitterData data;            ///< コピーされたエミッターデータ
      EmitterType type;            ///< コピーされたエミッタータイプ
      std::string renderModelPath; ///< 描画モデルのファイルパス (data_ 外メンバのため別途保持。空=描画モデル無し)
    };
    std::array<CopiedSettings, 5> copiedSettingsSlots_; ///< コピーバッファ（5スロット分）

    /// <summary>
    /// フォースフィールドマネージャへの弱参照（シーン統合保存連携用、nullable）
    /// </summary>
    ForceFieldManager* forceFieldManager_ = nullptr;

  };

} // namespace Tako