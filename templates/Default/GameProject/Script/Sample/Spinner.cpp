#include "Spinner.h"
#include "Actor.h"

void Spinner::Tick(float deltaTime) {
    GetOwner()->GetTransform().rotate += axis_ * (speed_ * deltaTime);
}
