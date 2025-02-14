#include <variant>
#include <map>
#include <string>
#include "Vec3Func.h"

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

	//-----------------------------------------Setter-----------------------------------------//
	// 値の設定(int)
	void SetValue(const std::string& groupName, const std::string& key, int32_t value);
	// 値の設定(float)
	void SetValue(const std::string& groupName, const std::string& key, float value);
	// 値の設定(Vector3)
	void SetValue(const std::string& groupName, const std::string& key, const Vector3& value);

public: // 構造体
	struct Item {
		std::variant<int32_t, float, Vector3> value;
	};

	struct Group {
		std::map<std::string, Item> items;
	};

private: // メンバ変数
	std::map<std::string, Group> datas_;
};