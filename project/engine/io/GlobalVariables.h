#pragma once
#include <variant>
#include <map>
#include <cassert>
#include <string>
#include "Vec3Func.h"
#include "Vector4.h"
#include "Vector2.h"

namespace Tako {

/// <summary>
/// グローバル変数管理クラス
/// JSON形式での設定値保存・読み込みとImGuiでの実行時編集
/// </summary>
class GlobalVariables
{
private: // シングルトン設定
  static GlobalVariables* instance_;
  GlobalVariables() = default;
  GlobalVariables(const GlobalVariables&) = delete;
  GlobalVariables& operator=(const GlobalVariables&) = delete;
  ~GlobalVariables() = default;

public: // メンバ関数
  /// <summary>
  /// インスタンスの取得
  /// </summary>
  static GlobalVariables* GetInstance();

  /// <summary>
  /// グループの作成
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void CreateGroup(const std::string& groupName);

  /// <summary>
  /// グループの削除
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void DeleteGroup(const std::string& groupName);

  /// <summary>
  /// 更新処理
  /// </summary>
  void Update();

  /// <summary>
  /// ファイルに書き出す
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void SaveFile(const std::string& groupName);

  /// <summary>
  /// ディレクトリの全ファイルを読み込む
  /// </summary>
  void LoadFiles();

  /// <summary>
  /// ファイルから読み込む
  /// </summary>
  /// <param name="groupName">グループ名</param>
  void LoadFile(const std::string& groupName);

  //-----------------------------------------Setter-----------------------------------------//
  /// <summary>
  /// 値の設定(int)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, int32_t value);

  /// <summary>
  /// 値の設定(float)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, float value);

  /// <summary>
  /// 値の設定(Vector2)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, const Vector2& value);

  /// <summary>
  /// 値の設定(Vector3)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, const Vector3& value);

  /// <summary>
  /// 値の設定(Vector4)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, const Vector4& value);

  /// <summary>
  /// 値の設定(bool)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void SetValue(const std::string& groupName, const std::string& key, bool value);

  /// <summary>
  /// 項目の追加(int)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, int32_t value);

  /// <summary>
  /// 項目の追加(float)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, float value);

  /// <summary>
  /// 項目の追加(Vector2)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, const Vector2& value);

  /// <summary>
  /// 項目の追加(Vector3)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, const Vector3& value);

  /// <summary>
  /// 項目の追加(Vector4)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, const Vector4& value);

  /// <summary>
  /// 項目の追加(bool)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <param name="value">設定する値</param>
  void AddItem(const std::string& groupName, const std::string& key, bool value);

  //-----------------------------------------Getter-----------------------------------------//
  /// <summary>
  /// 値の取得(int)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したint値</returns>
  int32_t GetValueInt(const std::string& groupName, const std::string& key);

  /// <summary>
  /// 値の取得(float)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したfloat値</returns>
  float GetValueFloat(const std::string& groupName, const std::string& key);

  /// <summary>
  /// 値の取得(Vector2)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したVector2値</returns>
  Vector2 GetValueVec2(const std::string& groupName, const std::string& key);

  /// <summary>
  /// 値の取得(Vector3)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したVector3値</returns>
  Vector3 GetValueVec3(const std::string& groupName, const std::string& key);

  /// <summary>
  /// 値の取得(Vector4)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したVector4値</returns>
  Vector4 GetValueVec4(const std::string& groupName, const std::string& key);

  /// <summary>
  /// 値の取得(bool)
  /// </summary>
  /// <param name="groupName">グループ名</param>
  /// <param name="key">キー名</param>
  /// <returns>取得したbool値</returns>
  bool GetValueBool(const std::string& groupName, const std::string& key);

  /// <summary>
  /// グループが登録されているか確認
  /// </summary>
  /// <returns>グループが1つ以上あればtrue</returns>
  bool HasGroups() const { return !datas_.empty(); }

public: // 構造体
  /// <summary>
  /// グローバル変数の単一アイテム
  /// </summary>
  struct Item {
    std::variant<bool, int32_t, float, Vector2, Vector3, Vector4> value; ///< 値（複数の型に対応）
  };

  /// <summary>
  /// グローバル変数のグループ
  /// </summary>
  struct Group {
    std::map<std::string, Item> items; ///< アイテムのマップ
  };

private: // メンバ変数

  const std::string kDirectoryPath = "resources/Json/GlobalVariables/"; ///< 保存先のファイルパス

  std::map<std::string, Group> datas_; ///< グループのマップ
};

} // namespace Tako
