#include "vesc_control/servo_controller.hpp"

#include <algorithm>

namespace vesc_control
{

ServoController::ServoController(
    VescInterface & vesc)
    : vesc_(vesc),
      current_position_(0.5f)
{
}


float ServoController::limitPosition(
    float position) const
{
    return std::clamp(
        position,
        MIN_POSITION,
        MAX_POSITION);
}


bool ServoController::setPosition(
    float position)
{
    position =
        limitPosition(position);

    if (!vesc_.setServoPosition(position)) {
        return false;
    }

    current_position_ =
        position;

    return true;
}


float ServoController::getPosition() const
{
    return current_position_;
}

}  // namespace vesc_control
