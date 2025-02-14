#include "GlobalVariables.h"
#include "ImGuiManager.h"
#include <cassert>

GlobalVariables* GlobalVariables::instance_ = nullptr;

GlobalVariables* GlobalVariables::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new GlobalVariables();
	}
	return instance_;
}

void GlobalVariables::CreateGroup(const std::string& groupName)
{
	datas_[groupName];
}

void GlobalVariables::Update()
{
	if (!ImGui::Begin("GlobalVariables", nullptr, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if (!ImGui::BeginMenuBar()) return;

	// 各グループの処理
	for (std::map<std::string, Group>::iterator itGroup = datas_.begin(); itGroup != datas_.end(); ++itGroup)
	{
		// グループ名を取得
    const std::string& groupName = itGroup->first;
    // グループの参照を取得
    Group& group = itGroup->second;

    if (!ImGui::BeginMenu(groupName.c_str())) continue;

    // 各アイテムの処理
    for (std::map<std::string, Item>::iterator itItem = group.items.begin(); itItem != group.items.end(); ++itItem)
    {
      // アイテム名を取得
      const std::string& itemName = itItem->first;
      // アイテムの参照を取得
      Item& item = itItem->second;

      // 型によって処理を分岐
      if (std::holds_alternative<int32_t>(item.value))
      {
        int32_t* value = std::get_if<int32_t>(&item.value);
        ImGui::DragInt(itemName.c_str(), value, 1);
      }
      else if (std::holds_alternative<float>(item.value))
      {
        float* value = std::get_if<float>(&item.value);
        ImGui::DragFloat(itemName.c_str(), value, 0.1f);
      }
      else if (std::holds_alternative<Vector3>(item.value))
      {
        Vector3* value = std::get_if<Vector3>(&item.value);
        ImGui::DragFloat3(itemName.c_str(), reinterpret_cast<float*>(value), 0.1f);
      }
    }

    ImGui::EndMenu();
	}
  
  ImGui::EndMenuBar();
  ImGui::End();
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, int32_t value)
{
  // グループが存在しない場合はエラー
  assert(datas_.find(groupName) != datas_.end());

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, float value)
{
	assert(datas_.find(groupName) != datas_.end());

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Vector3& value)
{
	assert(datas_.find(groupName) != datas_.end());

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}
