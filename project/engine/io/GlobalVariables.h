#include <variant>
#include <map>
#include <cassert>
#include <string>
#include "Vec3Func.h"
#include "Vector4.h"

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
  void CreateGroup(const std::string& groupName);

  /// <summary>
  /// グループの削除
  /// </summary>
  void DeleteGroup(const std::string& groupName);

  /// <summary>
  /// 更新処理
  /// </summary>
  void Update();

  /// <summary>
  /// ファイルに書き出す
  /// </summary>
  void SaveFile(const std::string& groupName);

  /// <summary>
  /// ディレクトリの全ファイルを読み込む
  /// </summary>
  void LoadFiles();

  /// <summary>
  /// ファイルから読み込む
  /// </summary>
  void LoadFile(const std::string& groupName);

  //-----------------------------------------Setter-----------------------------------------//
  // 値の設定(int)
  void SetValue(const std::string& groupName, const std::string& key, int32_t value);
  // 値の設定(float)
  void SetValue(const std::string& groupName, const std::string& key, float value);
  // 値の設定(Vector3)
  void SetValue(const std::string& groupName, const std::string& key, const Vector3& value);
  // 値の設定(Vector4)
  void SetValue(const std::string& groupName, const std::string& key, const Vector4& value);
  // 値の設定(bool)
  void SetValue(const std::string& groupName, const std::string& key, bool value);

  // 項目の追加(int)
  void AddItem(const std::string& groupName, const std::string& key, int32_t value);
  // 項目の追加(float)
  void AddItem(const std::string& groupName, const std::string& key, float value);
  // 項目の追加(Vector3)
  void AddItem(const std::string& groupName, const std::string& key, const Vector3& value);
  // 項目の追加(Vector4)
  void AddItem(const std::string& groupName, const std::string& key, const Vector4& value);
  // 項目の追加(bool)
  void AddItem(const std::string& groupName, const std::string& key, bool value);

  //-----------------------------------------Getter-----------------------------------------//
  // 値の取得(int)
  int32_t GetValueInt(const std::string& groupName, const std::string& key);
  // 値の取得(float)
  float GetValueFloat(const std::string& groupName, const std::string& key);
  // 値の取得(Vector3)
  Vector3 GetValueVec3(const std::string& groupName, const std::string& key);
  // 値の取得(Vector4)
  Vector4 GetValueVec4(const std::string& groupName, const std::string& key);
  // 値の取得(bool)
  bool GetValueBool(const std::string& groupName, const std::string& key);

public: // 構造体
  struct Item {
    std::variant<bool, int32_t, float, Vector3, Vector4> value;
  };

  struct Group {
    std::map<std::string, Item> items;
  };

private: // メンバ変数

  // 保存先のファイルパス
  const std::string kDirectoryPath = "resources/Json/GlobalVariables/";

  std::map<std::string, Group> datas_;
};


