#ifndef VESC_CONTROL__MOTOR_CONTROLLER_HPP_
#define VESC_CONTROL__MOTOR_CONTROLLER_HPP_

#include "vesc_control/vesc_interface.hpp"

#include <cstdint>

namespace vesc_control
{

class MotorController
{
public:
    /*
     * invert compensa el orden de las fases del motor.
     *
     * En un BLDC el sentido de giro depende de cómo estén
     * conectados los tres cables de fase, así que con este
     * cableado un ERPM positivo salía al revés. Con invert
     * en true, positivo = adelante.
     *
     * La alternativa física es intercambiar dos fases
     * cualesquiera y dejar esto en false.
     */
    explicit MotorController(
        VescInterface & vesc,
        bool invert = false);

    bool setErpm(int32_t erpm);

    bool stop();

    /*
     * Reenvía el último ERPM comandado.
     *
     * El firmware de la VESC tiene un timeout de seguridad
     * (timeout_msec, 1000 ms por defecto): si no recibe otro
     * comando dentro de esa ventana suelta el motor. Hay que
     * llamar a esto periódicamente para que siga girando.
     *
     * No hace nada si el motor está parado.
     */
    bool keepAlive();

    bool isActive() const;

    int32_t getErpm() const;

    // Límite de seguridad. Los comandos se recortan a ±MAX_ERPM.
    static constexpr int32_t MAX_ERPM = 20000;

private:
    VescInterface & vesc_;

    int32_t current_erpm_;

    // true mientras haya un ERPM distinto de cero comandado.
    bool active_;

    const bool invert_;

    int32_t limitErpm(int32_t erpm) const;

    /*
     * ERPM tal y como se manda a la VESC.
     *
     * current_erpm_ guarda siempre el valor lógico que pidió
     * el usuario; la inversión se aplica sólo al enviar, para
     * que los logs y getErpm() coincidan con lo publicado.
     */
    int32_t outputErpm() const;
};

}  // namespace vesc_control

#endif