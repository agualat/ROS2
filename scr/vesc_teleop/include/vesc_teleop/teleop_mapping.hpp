#ifndef VESC_TELEOP__TELEOP_MAPPING_HPP_
#define VESC_TELEOP__TELEOP_MAPPING_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vesc_teleop
{

/*
 * Índices por defecto de game_controller_node (paquete joy),
 * que usa el mapeo estándar de SDL: es el mismo para mandos
 * Xbox, PS4, PS5, 8BitDo, etc.
 *
 * Ejes (positivo = arriba / izquierda):
 *   0 stick izq. X, 1 stick izq. Y, 2 stick der. X, 3 stick der. Y
 * Botones:
 *   9 LB / L1, 10 RB / R1
 */
struct TeleopConfig
{
    int deadman_button = 9;
    int turbo_button = 10;
    int throttle_axis = 1;
    int steering_axis = 2;

    int32_t max_erpm = 5000;
    int32_t turbo_max_erpm = 10000;
    bool invert_throttle = false;

    float steering_center = 0.5f;
    float steering_range = 0.4f;
    bool invert_steering = false;
};


struct Command
{
    int32_t erpm = 0;
    float servo = 0.5f;
};


inline bool pressed(
    const std::vector<int32_t> & buttons,
    int index)
{
    return index >= 0 &&
        static_cast<size_t>(index) < buttons.size() &&
        buttons[index] != 0;
}


inline float axisValue(
    const std::vector<float> & axes,
    int index)
{
    if (index < 0 || static_cast<size_t>(index) >= axes.size()) {
        return 0.0f;
    }
    const float value = axes[index];
    return std::isfinite(value)
        ? std::clamp(value, -1.0f, 1.0f)
        : 0.0f;
}


/*
 * Sin el botón de hombre muerto pulsado: motor libre
 * (ERPM 0) y dirección al centro.
 */
inline Command computeCommand(
    const TeleopConfig & config,
    const std::vector<float> & axes,
    const std::vector<int32_t> & buttons)
{
    Command command;
    command.servo = config.steering_center;

    if (!pressed(buttons, config.deadman_button)) {
        return command;
    }

    const int32_t limit =
        pressed(buttons, config.turbo_button)
            ? config.turbo_max_erpm
            : config.max_erpm;

    float throttle = axisValue(axes, config.throttle_axis);
    if (config.invert_throttle) {
        throttle = -throttle;
    }

    float steering = axisValue(axes, config.steering_axis);
    if (config.invert_steering) {
        steering = -steering;
    }

    command.erpm = static_cast<int32_t>(
        std::lround(throttle * static_cast<float>(limit)));

    command.servo = std::clamp(
        config.steering_center + steering * config.steering_range,
        0.0f,
        1.0f);

    return command;
}

}  // namespace vesc_teleop

#endif
