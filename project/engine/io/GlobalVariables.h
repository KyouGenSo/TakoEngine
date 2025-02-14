#include <variant>
#include <map>
#include <string>
#include "Vec3Func.h"

class GlobalVariables
{
	

private: // �V���O���g���ݒ�
	static GlobalVariables* instance_;
	GlobalVariables() = default;
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;
	~GlobalVariables() = default;

public: // �����o�֐�
	/// <summary>
	/// �C���X�^���X�̎擾
	/// </summary>
	static GlobalVariables* GetInstance();

	/// <summary>
	/// �O���[�v�̍쐬
	/// </summary>
	void CreateGroup(const std::string& groupName);

	//-----------------------------------------Setter-----------------------------------------//
	// �l�̐ݒ�(int)
	void SetValue(const std::string& groupName, const std::string& key, int32_t value);
	// �l�̐ݒ�(float)
	void SetValue(const std::string& groupName, const std::string& key, float value);
	// �l�̐ݒ�(Vector3)
	void SetValue(const std::string& groupName, const std::string& key, const Vector3& value);

public: // �\����
	struct Item {
		std::variant<int32_t, float, Vector3> value;
	};

	struct Group {
		std::map<std::string, Item> items;
	};

private: // �����o�ϐ�
	std::map<std::string, Group> datas_;
};