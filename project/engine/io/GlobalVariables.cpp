#include "GlobalVariables.h"
#include <cassert>
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

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, int32_t value)
{
	assert(datas_.find(groupName) != datas_.end(), "GlobalVariable Group is not exist");

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, float value)
{
	assert(datas_.find(groupName) != datas_.end(), "GlobalVariable Group is not exist");

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}

void GlobalVariables::SetValue(const std::string& groupName, const std::string& key, const Vector3& value)
{
	assert(datas_.find(groupName) != datas_.end(), "GlobalVariable Group is not exist");

	Group& group = datas_[groupName];

	Item newItem{};
	newItem.value = value;

	group.items[key] = newItem;
}
