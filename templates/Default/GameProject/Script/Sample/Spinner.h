#pragma once
#include "Script.h"
#include "Vector3.h"

#include <numbers>

/// <summary>
/// Actor を一定の速さで回す
/// </summary>
TCLASS()
class Spinner : public Tako::Script {
    GENERATED_BODY()

public: //メンバー関数
    void Tick(float deltaTime) override;

private: //メンバー変数
    TPROPERTY(Edit, Angle, Tooltip = "1 秒あたりの回転（度）", Speed = 1)
    float speed_ = std::numbers::pi_v<float>;
    TPROPERTY(Edit, Tooltip = "回す軸。成分ごとに速さを掛ける", Speed = 0.05)
    Tako::Vector3 axis_{ 0.0f, 1.0f, 0.0f };
};
