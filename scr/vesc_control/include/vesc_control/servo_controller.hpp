#ifndef VESC_CONTROL__SERVO_CONTROLLER_HPP_
#define VESC_CONTROL__SERVO_CONTROLLER_HPP_

#include "vesc_control/vesc_interface.hpp"

namespace vesc_control
{

class ServoController
{
public:
    explicit ServoController(
        VescInterface & vesc);

    bool setPosition(float position);

    float getPosition() const;

private:
    VescInterface & vesc_;

    float current_position_;

    static constexpr float MIN_POSITION = 0.1f;
    static constexpr float MAX_POSITION = 0.9f;

    float limitPosition(float position) const;
};

}  // namespace vesc_control

#endif