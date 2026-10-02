#include "vesc_teleop/teleop_mapping.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int checks = 0;
void require(bool condition, const std::string & message)
{
    ++checks;
    if (!condition) { throw std::runtime_error(message); }
}
bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
}  // namespace

int main()
{
    using namespace vesc_teleop;
    try {
        const TeleopConfig config;
        std::vector<float> axes(6, 0.0f);
        std::vector<int32_t> buttons(16, 0);

        axes[1] = 1.0f;
        axes[2] = 1.0f;
        auto command = computeCommand(config, axes, buttons);
        require(command.erpm == 0, "Sin hombre muerto el motor no se mueve");
        require(near(command.servo, 0.5f), "Sin hombre muerto la direccion se centra");

        buttons[9] = 1;
        command = computeCommand(config, axes, buttons);
        require(command.erpm == 5000, "Stick arriba = max_erpm adelante");
        require(near(command.servo, 0.9f), "Stick a tope = limite del servo");

        axes[1] = -0.5f;
        axes[2] = -1.0f;
        command = computeCommand(config, axes, buttons);
        require(command.erpm == -2500, "Media marcha atras");
        require(near(command.servo, 0.1f), "Direccion al otro lado");

        buttons[10] = 1;
        axes[1] = 1.0f;
        require(computeCommand(config, axes, buttons).erpm == 10000, "Turbo");

        axes[1] = 3.0f;
        require(computeCommand(config, axes, buttons).erpm == 10000, "Eje fuera de rango se recorta");
        axes[1] = std::numeric_limits<float>::quiet_NaN();
        require(computeCommand(config, axes, buttons).erpm == 0, "NaN no mueve el motor");

        TeleopConfig inverted;
        inverted.invert_throttle = true;
        inverted.invert_steering = true;
        axes[1] = 1.0f;
        axes[2] = 1.0f;
        buttons[10] = 0;
        command = computeCommand(inverted, axes, buttons);
        require(command.erpm == -5000 && near(command.servo, 0.1f), "Inversion");

        require(computeCommand(config, {}, {}).erpm == 0, "Mensaje vacio no mueve");
        require(computeCommand(config, {}, buttons).erpm == 0, "Ejes ausentes no mueven");

        TeleopConfig bad;
        bad.deadman_button = 40;
        bad.throttle_axis = -1;
        require(computeCommand(bad, axes, buttons).erpm == 0, "Indices invalidos no mueven");

        std::cout << checks << " teleop-mapping checks passed\n";
        return 0;
    } catch (const std::exception & error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
