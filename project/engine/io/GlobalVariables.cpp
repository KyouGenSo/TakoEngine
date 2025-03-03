#include "GlobalVariables.h"
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

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

void GlobalVariables::DeleteGroup(const std::string& groupName)
{
  datas_.erase(groupName);
}

void GlobalVariables::Update()
{
#ifdef _DEBUG
  // グループが存在しない場合は早期リターン
  if (datas_.empty()) return;

	if (!ImGui::Begin("Variables", nullptr, ImGuiWindowFlags_MenuBar))
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
      if (std::holds_alternative<bool>(item.value))
      {
        bool* value = std::get_if<bool>(&item.value);
        ImGui::Checkbox(itemName.c_str(), value);
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
      else if (std::holds_alternative<Vector4>(item.value))
      {
        Vector4* value = std::get_if<Vector4>(&item.value);
        ImGui::ColorEdit4(itemName.c_str(), reinterpret_cast<float*>(value));
      }
      else if (std::holds_alternative<int32_t>(item.value))
      {
        int32_t* value = std::get_if<int32_t>(&item.value);
        ImGui::DragInt(itemName.c_str(), value, 1);
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
#endif
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
    if (std::holds_alternative<bool>(item.value))
    {
      root[groupName][itemName] = std::get<bool>(item.value);
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
    else if (std::holds_alternative<Vector4>(item.value))
    {
      Vector4 value = std::get<Vector4>(item.value);
      root[groupName][itemName] = json::array({ value.x, value.y, value.z, value.w });
    }
    else if (std::holds_alternative<int32_t>(item.value))
    {
      root[groupName][itemName] = std::get<int32_t>(item.value);
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
#ifdef _DEBUG
    MessageBoxA(nullptr, erroeMessage.c_str(), "GlobalVariables", 0);
#endif
    assert(false);
    return;
  }

  // ファイルにjson文字列を書き込み(インデント4)
  ofs << std::setw(4) << root << std::endl;

  // ファイルを閉じる
  ofs.close();

}

void GlobalVariables::LoadFiles()
{
  // ディレクトリが存在しない場合はスキップ
  if (!std::filesystem::exists(kDirectoryPath))
  {
    return;
  }

  std::filesystem::directory_iterator dir_it(kDirectoryPath);
  for (const std::filesystem::directory_entry& entry : dir_it)
  {
    // ファイルパスをs取得
    const std::filesystem::path& filePath = entry.path();

    // ファイル拡張子を取得
    std::string extension = filePath.extension().string();

    // 拡張子が.jsonでない場合はスキップ
    if (extension.compare(".json") != 0)
    {
      continue;
    }

    // ファイル読み込み
    LoadFile(filePath.stem().string());

  }
}

void GlobalVariables::LoadFile(const std::string& groupName)
{
  std::string filePath = kDirectoryPath + groupName + ".json";

  // 読み込み用のファイルストリーム
  std::ifstream ifs;

  // ファイルを開く
  ifs.open(filePath);

  // エラーハンドル
  if (ifs.fail())
  {
    std::string erroeMessage = "fail to opne json file";
#ifdef _DEBUG
    MessageBoxA(nullptr, erroeMessage.c_str(), "GlobalVariables", 0);
#endif
    assert(false);
    return;
  }

  json root;

  // ファイルから読み込み
  ifs >> root;

  // ファイルを閉じる
  ifs.close();

  // グループが存在しない場合はエラー
  json::iterator itGroup = root.find(groupName);
  assert(itGroup != root.end());

  // 各アイテムの処理
  for (json::iterator itItem = itGroup->begin(); itItem != itGroup->end(); ++itItem)
  {
    // アイテム名を取得
    const std::string& itemName = itItem.key();

    // 型によって処理を分岐
    if (itItem->is_number_integer())
    {
      int32_t value = itItem->get<int32_t>();
      SetValue(groupName, itemName, value);
    }
    else if (itItem->is_number_float())
    {
      double value = itItem->get<double>();
      SetValue(groupName, itemName, static_cast<float>(value));
    }
    else if (itItem->is_array() && itItem->size() == 3)
    {
      Vector3 value = { itItem->at(0), itItem->at(1), itItem->at(2) };
      SetValue(groupName, itemName, value);
    }
    else if (itItem->is_array() && itItem->size() == 4)
    {
      Vector4 value = { itItem->at(0), itItem->at(1), itItem->at(2), itItem->at(3) };
      SetValue(groupName, itemName, value);
    }
    else if (itItem->is_boolean())
    {
      bool value = itItem->get<bool>();
      SetValue(groupName, itemName, value);
    }
  }

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

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Vector4& value)
{
  assert(datas_.find(groupName) != datas_.end());

  Group& group = datas_[groupName];

  Item newItem{};
  newItem.value = value;

  group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, bool value)
{
  assert(datas_.find(groupName) != datas_.end());
  Group& group = datas_[groupName];

  Item newItem{};
  // 明示的に bool 型としてインプレース構築
  newItem.value.emplace<bool>(value);

  group.items[key] = newItem;
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, int32_t value)
{
  // 項目が存在しない場合は追加、存在する場合は何もしない
  if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
  {
    SetValue(groupName, key, value);
  }
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, float value)
{
  if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
  {
    SetValue(groupName, key, value);
  }
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector3& value)
{
  if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
  {
    SetValue(groupName, key, value);
  }
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector4& value)
{
  if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
  {
    SetValue(groupName, key, value);
  }
}

void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, bool value)
{
  if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
  {
    SetValue(groupName, key, value);
  }
}

int32_t GlobalVariables::GetValueInt(const std::string& groupName, const std::string& key)
{
  // グループが存在しない場合はエラー
  assert(datas_.find(groupName) != datas_.end());
  const Group& group = datas_.at(groupName);
  // 項目が存在しない場合はエラー
  assert(group.items.find(key) != group.items.end());
  const Item& item = group.items.at(key);
  // 型がintでない場合はエラー
  assert(std::holds_alternative<int32_t>(item.value));
  return std::get<int32_t>(item.value);
}

float GlobalVariables::GetValueFloat(const std::string& groupName, const std::string& key)
{
  assert(datas_.find(groupName) != datas_.end());
  const Group& group = datas_.at(groupName);
  assert(group.items.find(key) != group.items.end());
  const Item& item = group.items.at(key);
  assert(std::holds_alternative<float>(item.value));
  return std::get<float>(item.value);
}

Vector3 GlobalVariables::GetValueVec3(const std::string& groupName, const std::string& key)
{
  assert(datas_.find(groupName) != datas_.end());
  const Group& group = datas_.at(groupName);
  assert(group.items.find(key) != group.items.end());
  const Item& item = group.items.at(key);
  assert(std::holds_alternative<Vector3>(item.value));
  return std::get<Vector3>(item.value);
}

Vector4 GlobalVariables::GetValueVec4(const std::string& groupName, const std::string& key)
{
  assert(datas_.find(groupName) != datas_.end());
  const Group& group = datas_.at(groupName);
  assert(group.items.find(key) != group.items.end());
  const Item& item = group.items.at(key);
  assert(std::holds_alternative<Vector4>(item.value));
  return std::get<Vector4>(item.value);
}

bool GlobalVariables::GetValueBool(const std::string& groupName, const std::string& key)
{
  // グループの存在を確認
  auto groupIt = datas_.find(groupName);
  assert(groupIt != datas_.end());
  const Group& group = groupIt->second;

  // キーが存在するか確認
  auto itemIt = group.items.find(key);
  assert(itemIt != group.items.end());
  const Item& item = itemIt->second;

  // bool型が格納されているか確認
  if (auto boolPtr = std::get_if<bool>(&item.value)) {
    return *boolPtr;
  }
  // もしint32_t型が格納されている場合は、0以外ならtrueとみなす
  else if (auto intPtr = std::get_if<int32_t>(&item.value)) {
    return (*intPtr != 0);
  }
  // 他の型の場合は例外をスロー（もしくはエラーハンドリングを行う）
  throw std::runtime_error("Variant does not contain a bool or convertible int32_t value.");
}
