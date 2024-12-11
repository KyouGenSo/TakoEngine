#pragma once
#include "Vector"
#include <algorithm>

class CollisionRecord {

private:
	std::vector<uint32_t> collisionRecords_;

public:
	void AddRecord(uint32_t number);

	bool CheckRecord(uint32_t number);

	void Clear();
};