#include "GlobalVariables.h"
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

using json = nlohmann::json;

namespace Tako {

  namespace {

    /// <summary>
    /// SaveFile が書き込む内容
    /// </summary>
    json SerializeGroup(const std::string& groupName, const GlobalVariables::Group& group) {
      json root = json::object();
      root[groupName] = json::object();

      for (const auto& [itemName, item] : group.items) {
        // 型に応じて json へ書き出し
        if (std::holds_alternative<bool>(item.value)) {
          root[groupName][itemName] = std::get<bool>(item.value);
        }
        else if (std::holds_alternative<float>(item.value)) {
          root[groupName][itemName] = std::get<float>(item.value);
        }
        else if (std::holds_alternative<Vector2>(item.value)) {
          Vector2 value = std::get<Vector2>(item.value);
          root[groupName][itemName] = json::array({ value.x, value.y });
        }
        else if (std::holds_alternative<Vector3>(item.value)) {
          Vector3 value = std::get<Vector3>(item.value);
          root[groupName][itemName] = json::array({ value.x, value.y, value.z });
        }
        else if (std::holds_alternative<Vector4>(item.value)) {
          Vector4 value = std::get<Vector4>(item.value);
          root[groupName][itemName] = json::array({ value.x, value.y, value.z, value.w });
        }
        else if (std::holds_alternative<int32_t>(item.value)) {
          root[groupName][itemName] = std::get<int32_t>(item.value);
        }
      }
      return root;
    }

    /// <summary>
    /// SerializeGroup の 1 グループ分の値を設定する。json の型から値の型を判別する（配列は要素数で Vector2/3/4 を区別）
    /// </summary>
    void ApplyGroup(GlobalVariables& variables, const std::string& groupName, const json& items) {
      for (json::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem) {
        const std::string& itemName = itItem.key();

        if (itItem->is_number_integer()) {
          variables.SetValue(groupName, itemName, itItem->get<int32_t>());
        }
        else if (itItem->is_number_float()) {
          variables.SetValue(groupName, itemName, static_cast<float>(itItem->get<double>()));
        }
        else if (itItem->is_array() && itItem->size() == 2) {
          variables.SetValue(groupName, itemName, Vector2{ itItem->at(0), itItem->at(1) });
        }
        else if (itItem->is_array() && itItem->size() == 3) {
          variables.SetValue(groupName, itemName, Vector3{ itItem->at(0), itItem->at(1), itItem->at(2) });
        }
        else if (itItem->is_array() && itItem->size() == 4) {
          variables.SetValue(groupName, itemName, Vector4{ itItem->at(0), itItem->at(1), itItem->at(2), itItem->at(3) });
        }
        else if (itItem->is_boolean()) {
          variables.SetValue(groupName, itemName, itItem->get<bool>());
        }
      }
    }

  } // namespace

  std::unique_ptr<GlobalVariables> GlobalVariables::instance_ = nullptr;

  GlobalVariables* GlobalVariables::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<GlobalVariables>(Token{});
    }
    return instance_.get();
  }

  void GlobalVariables::CreateGroup(const std::string& groupName)
  {
    datas_[groupName];
  }

  void GlobalVariables::DeleteGroup(const std::string& groupName)
  {
    datas_.erase(groupName);
  }

#ifdef _DEBUG
  std::span<const ShortcutBinding<GlobalVariables>> GlobalVariables::Shortcuts()
  {
    static constexpr ShortcutBinding<GlobalVariables> kShortcuts[] = {
      // 開いているグループの区別がないので未保存のグループをすべて保存する。保存のたびに操作が止まらないよう、メッセージボックスは出さない
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the changed groups", [](GlobalVariables& variables) {
        for (const auto& [groupName, group] : variables.datas_) {
          if (group.isDirty) {
            variables.SaveFile(groupName);
          }
        }
      } },
    };
    return kShortcuts;
  }
#endif

  void GlobalVariables::Update()
  {
#ifdef _DEBUG
    // グループが存在しない場合は早期リターン
    if (datas_.empty()) return;

    if (!ImGui::Begin("Variables")) {
      ImGui::End();
      return;
    }

    // UI を描く区間の前後で全グループの値を比べ、Undo の 1 手にする
    editTracker_.BeginSection();
    editTracker_.DrawButtons();

    // 各グループの処理
    for (std::map<std::string, Group>::iterator itGroup = datas_.begin(); itGroup != datas_.end(); ++itGroup) {
      const std::string& groupName = itGroup->first;
      Group& group = itGroup->second;

      // 未保存の印でラベルが変わっても開閉状態を保つよう、ID はグループ名で固定する
      const std::string header = std::format("{}{}###{}", groupName, group.isDirty ? " *" : "", groupName);
      if (ImGui::CollapsingHeader(header.c_str())) {
        ImGui::Indent();

        for (std::map<std::string, Item>::iterator itItem = group.items.begin(); itItem != group.items.end(); ++itItem) {
          const std::string& itemName = itItem->first;
          Item& item = itItem->second;

          // 型によって ImGui ウィジェットを切り替え
          bool isEdited = false;
          if (std::holds_alternative<bool>(item.value)) {
            bool* value = std::get_if<bool>(&item.value);
            isEdited = ImGui::Checkbox(itemName.c_str(), value);
          }
          else if (std::holds_alternative<float>(item.value)) {
            float* value = std::get_if<float>(&item.value);
            isEdited = ImGui::DragFloat(itemName.c_str(), value, 0.1f);
          }
          else if (std::holds_alternative<Vector2>(item.value)) {
            Vector2* value = std::get_if<Vector2>(&item.value);
            isEdited = ImGui::DragFloat2(itemName.c_str(), reinterpret_cast<float*>(value), 0.1f);
          }
          else if (std::holds_alternative<Vector3>(item.value)) {
            Vector3* value = std::get_if<Vector3>(&item.value);
            isEdited = ImGui::DragFloat3(itemName.c_str(), reinterpret_cast<float*>(value), 0.1f);
          }
          else if (std::holds_alternative<Vector4>(item.value)) {
            Vector4* value = std::get_if<Vector4>(&item.value);
            isEdited = ImGui::ColorEdit4(itemName.c_str(), reinterpret_cast<float*>(value));
          }
          else if (std::holds_alternative<int32_t>(item.value)) {
            int32_t* value = std::get_if<int32_t>(&item.value);
            isEdited = ImGui::DragInt(itemName.c_str(), value, 1);
          }
          // 編集で、最後に保存・読込した時と同じ内容に戻ったら未保存を外す
          if (isEdited) {
            group.isDirty = SerializeGroup(groupName, group).dump() != group.savedSnapshot;
          }
        }

        ImGui::Text("\n");

        if (ImGui::Button(("Save##" + groupName).c_str())) {
          SaveFile(groupName);
          std::string message = std::format("{}.json saved", groupName);
          MessageBoxA(nullptr, message.c_str(), "GlobalVariables", 0);
        }

        ImGui::Unindent();
      }
    }

    ProcessShortcuts(Shortcuts(), *this);

    editTracker_.EndSection();
    ImGui::End();
#endif
  }

  void GlobalVariables::SaveFile(const std::string& groupName)
  {
    // グループが存在しない場合はエラー
    std::map<std::string, Group>::iterator itGroup = datas_.find(groupName);
    assert(itGroup != datas_.end());

    const json root = SerializeGroup(groupName, itGroup->second);

    // ディレクトリが存在しない場合は作成
    std::filesystem::path directoryPath(kDirectoryPath);
    if (!std::filesystem::exists(directoryPath)) {
      std::filesystem::create_directories(directoryPath);
    }

    // ファイルに書き込み
    std::string filePath = kDirectoryPath + groupName + ".json";
    std::ofstream ofs;
    ofs.open(filePath);

    // エラーハンドル
    if (ofs.fail()) {
      std::string errorMessage = "file to opne json file";
#ifdef _DEBUG
      MessageBoxA(nullptr, errorMessage.c_str(), "GlobalVariables", 0);
#endif
      assert(false);
      return;
    }

    // ファイルに json 文字列を書き込み(インデント4)
    ofs << std::setw(4) << root << std::endl;

    // ファイルを閉じる
    ofs.close();

    itGroup->second.isDirty       = false;
    itGroup->second.savedSnapshot = root.dump();

  }

  void GlobalVariables::SaveAllFiles()
  {
    for (const auto& [groupName, group] : datas_) {
      SaveFile(groupName);
    }
  }

  void GlobalVariables::LoadFiles()
  {
    // ディレクトリが存在しない場合はスキップ
    if (!std::filesystem::exists(kDirectoryPath)) {
      return;
    }

    std::filesystem::directory_iterator dir_it(kDirectoryPath);
    for (const std::filesystem::directory_entry& entry : dir_it) {
      const std::filesystem::path& filePath = entry.path();

      std::string extension = filePath.extension().string();

      // .json 以外はスキップ
      if (extension != ".json") {
        continue;
      }

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
    if (ifs.fail()) {
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

    // グループを作成
    CreateGroup(groupName);
    ApplyGroup(*this, groupName, *itGroup);

    Group& group = datas_[groupName];
    group.isDirty       = false;
    group.savedSnapshot = SerializeGroup(groupName, group).dump();
  }

  std::string GlobalVariables::SerializeGroups() const
  {
    json root = json::object();
    for (const auto& [groupName, group] : datas_) {
      root.update(SerializeGroup(groupName, group));
    }
    return root.dump();
  }

  void GlobalVariables::RestoreGroups(const std::string& groups)
  {
    const json root = json::parse(groups);
    for (auto& [groupName, group] : datas_) {
      if (root.contains(groupName)) {
        ApplyGroup(*this, groupName, root[groupName]);
        group.isDirty = SerializeGroup(groupName, group).dump() != group.savedSnapshot;
      }
    }
  }

  void GlobalVariables::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    for (const auto& [groupName, group] : datas_) {
      if (group.isDirty) {
        out.push_back("Variables: " + groupName);
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

  void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Vector2& value)
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
    if (!datas_[groupName].items.contains(key)) {
      SetValue(groupName, key, value);
    }
  }

  void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, float value)
  {
    if (!datas_[groupName].items.contains(key)) {
      SetValue(groupName, key, value);
    }
  }

  void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector2& value)
  {
    if (!datas_[groupName].items.contains(key)) {
      SetValue(groupName, key, value);
    }
  }

  void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector3& value)
  {
    if (!datas_[groupName].items.contains(key)) {
      SetValue(groupName, key, value);
    }
  }

  void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, const Vector4& value)
  {
    if (!datas_[groupName].items.contains(key)) {
      SetValue(groupName, key, value);
    }
  }

  void GlobalVariables::AddItem(const std::string& groupName, const std::string& key, bool value)
  {
    if (!datas_[groupName].items.contains(key)) {
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
    // 型が int でない場合はエラー
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

  Vector2 GlobalVariables::GetValueVec2(const std::string& groupName, const std::string& key)
  {
    assert(datas_.find(groupName) != datas_.end());
    const Group& group = datas_.at(groupName);
    assert(group.items.find(key) != group.items.end());
    const Item& item = group.items.at(key);
    assert(std::holds_alternative<Vector2>(item.value));
    return std::get<Vector2>(item.value);
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

    // bool 型が格納されているか確認
    if (auto boolPtr = std::get_if<bool>(&item.value)) {
      return *boolPtr;
    }
    // もし int32_t 型が格納されている場合は、0以外なら true とみなす
    else if (auto intPtr = std::get_if<int32_t>(&item.value)) {
      return (*intPtr != 0);
    }
    // 他の型の場合は例外をスロー（もしくはエラーハンドリングを行う）
    throw std::runtime_error("Variant does not contain a bool or convertible int32_t value.");
  }

} // namespace Tako
