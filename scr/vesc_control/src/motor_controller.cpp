#include "vesc_control/motor_controller.hpp"

#include <algorithm>

namespace vesc_control
{

MotorController::MotorController(
    VescInterface & vesc,
    bool invert)
    : vesc_(vesc),
      current_erpm_(0),
      active_(false),
      invert_(invert)
{
}


int32_t MotorController::outputErpm() const
{
    return invert_
        ? -current_erpm_
        : current_erpm_;
}


int32_t MotorController::limitErpm(
    int32_t erpm) const
{
    return std::clamp(
        erpm,
        -MAX_ERPM,
        MAX_ERPM);
}


bool MotorController::setErpm(
    int32_t erpm)
{
    erpm =
        limitErpm(erpm);

    /*
     * ERPM 0 no se manda como consigna de velocidad:
     * eso haría que la VESC regule activamente contra
     * el cero (motor rígido, zumbido, consumo).
     *
     * Se libera el motor con corriente 0.
     */

    if (erpm == 0) {

        if (!vesc_.setCurrent(0.0f)) {
            return false;
        }

        current_erpm_ = 0;
        active_ = false;

        return true;
    }

    current_erpm_ =
        erpm;

    if (!vesc_.setErpm(outputErpm())) {

        current_erpm_ = 0;
        active_ = false;

        return false;
    }

    active_ = true;

    return true;
}


bool MotorController::keepAlive()
{
    if (!active_) {
        return true;
    }

    return vesc_.setErpm(outputErpm());
}


bool MotorController::isActive() const
{
    return active_;
}


bool MotorController::stop()
{
    return setErpm(0);
}


int32_t MotorController::getErpm() const
{
    return current_erpm_;
}

}  // namespace vesc_control