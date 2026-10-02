#ifndef VESC_CONTROL__PORT_DETECTION_HPP_
#define VESC_CONTROL__PORT_DETECTION_HPP_

#include <string>
#include <vector>

namespace vesc_control
{

struct SerialPortInfo
{
    std::string device;      // /dev/ttyACM0
    std::string vendor_id;   // 0483
    std::string product_id;  // 5740
    std::string product;     // ChibiOS/RT Virtual COM Port
};

/*
 * Lista los puertos ttyACM* / ttyUSB* con los datos USB
 * que publica el kernel en /sys/class/tty.
 */
std::vector<SerialPortInfo> listSerialPorts();

/*
 * Devuelve el puerto a usar.
 *
 * Si requested es una ruta (/dev/ttyACM1,
 * /dev/serial/by-id/...) se devuelve tal cual.
 *
 * Si es "auto" (o vacío) busca la VESC por VID:PID USB.
 * La VESC 6 con firmware ChibiOS se anuncia como
 * 0483:5740. Lanza std::runtime_error si no hay ninguna
 * o si hay varias, para no abrir un puerto equivocado.
 */
std::string resolvePort(
    const std::string & requested,
    const std::string & vendor_id,
    const std::string & product_id);

}  // namespace vesc_control

#endif
