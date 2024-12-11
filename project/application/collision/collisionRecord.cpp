#include "collisionRecord.h"

void CollisionRecord::AddRecord(uint32_t number) {
	collisionRecords_.push_back(number);
}

bool CollisionRecord::CheckRecord(uint32_t number) {
	// 重複チェック
	return std::any_of(collisionRecords_.begin(), collisionRecords_.end(), [number](uint32_t record) { return record == number; });
}

void CollisionRecord::Clear() {
	collisionRecords_.clear();
}