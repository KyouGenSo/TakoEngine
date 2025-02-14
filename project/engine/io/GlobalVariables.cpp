#include "GlobalVariables.h"
#include "ImGuiManager.h"
#include <cassert>
#include <json.hpp>
#include <filesystem>
#include <fstream>

GlobalVariables* GlobalVariables::instance_ = nullptr;

using json = nlohmann::json;

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

    ImGui::Text("\n");

    if (ImGui::Button("Save"))
    {
      SaveFile(groupName);
      std::string message = std::format("{}.json saved", groupName);
      MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
    }

    ImGui::EndMenu();
	}
  
  ImGui::EndMenuBar();
  ImGui::End();
}

void GlobalVariables::SaveFile(const std::string& groupName)
{
  // グループが存在しない場合はエラー
  std::map<std::string, Group>::iterator itGroup = datas_.find(groupName);
  assert(itGroup != datas_.end());

  json root;

  root = json::object();
  root[groupName] = json::object();

  // 各項目について
  for (std::map<std::string, Item>::iterator itItem = itGroup->second.items.begin(); itItem != itGroup->second.items.end(); ++itItem)
  {
    // アイテム名を取得
    const std::string& itemName = itItem->first;
    // アイテムの参照を取得
    Item& item = itItem->second;

    // 型によって処理を分岐
    if (std::holds_alternative<int32_t>(item.value))
    {
      root[groupName][itemName] = std::get<int32_t>(item.value);
    }
    else if (std::holds_alternative<float>(item.value))
    {
      root[groupName][itemName] = std::get<float>(item.value);
    }
    else if (std::holds_alternative<Vector3>(item.value))
    {
      Vector3 value = std::get<Vector3>(item.value);
      root[groupName][itemName] = json::array({ value.x, value.y, value.z });
    }
  }

  // ディレクトリが存在しない場合は作成
  std::filesystem::path directoryPath(kDirectoryPath);
  if (!std::filesystem::exists(directoryPath))
  {
    std::filesystem::create_directories(directoryPath);
  }

  // ファイルに書き込み
  std::string filePath = kDirectoryPath + groupName + ".json";
  std::ofstream ofs;
  ofs.open(filePath);

  // エラーハンドル
  if (ofs.fail())
  {
    std::string erroeMessage = "file to opne json file";
    MessageBoxA(nullptr, erroeMessage.c_str(), "GlobalVariables", 0);
    assert(false);
    return;
  }

  // ファイルにjson文字列を書き込み(インデント4)
  ofs << std::setw(4) << root << std::endl;

  // ファイルを閉じる
  ofs.close();

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
