#ifndef D435I_BRINGUP__DEVICE_SELECTION_HPP_
#define D435I_BRINGUP__DEVICE_SELECTION_HPP_

#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace d435i_bringup
{

struct Device
{
  std::string name;
  std::string serial;
  std::string usb_port;
  std::string physical_port;
  std::string usb_type;
};

inline bool is_d435i(const std::string & name)
{
  const std::regex pattern("\\bd435i\\b", std::regex::icase);
  return std::regex_search(name, pattern);
}

inline std::string usb_port_from_path(const std::string & path)
{
  const std::regex pattern("(?:^|/)([0-9]+-[0-9]+(?:\\.[0-9]+)*)(?=[:/]|$)");
  std::string port;
  for (auto cursor = std::sregex_iterator(path.begin(), path.end(), pattern);
    cursor != std::sregex_iterator(); ++cursor)
  {
    port = (*cursor)[1].str();
  }
  return port;
}

inline std::string normalize_serial(std::string serial)
{
  if (!serial.empty() && serial.front() == '_') {
    serial.erase(serial.begin());
  }
  if (serial.empty() || serial == "auto" || serial == "''") {
    return "";
  }
  if (serial.size() < 8 || !std::all_of(serial.begin(), serial.end(), [](unsigned char c) {
      return std::isdigit(c) != 0;
    }))
  {
    throw std::runtime_error(
      "Usa el numero de serie USB de la camara. K38179-111 es la etiqueta del modelo, "
      "no el numero de serie seleccionado por el driver.");
  }
  return serial;
}

inline Device select_device(
  const std::vector<Device> & devices,
  const std::string & requested_serial = "auto",
  const std::string & requested_port = "")
{
  const auto serial = normalize_serial(requested_serial);
  std::vector<Device> candidates;
  for (const auto & device : devices) {
    if (is_d435i(device.name) && (serial.empty() || device.serial == serial) &&
      (requested_port.empty() || device.usb_port == requested_port))
    {
      candidates.push_back(device);
    }
  }
  if (candidates.empty()) {
    throw std::runtime_error(
      "No se encontro una D435i coincidente. Comprueba el cable USB, permisos "
      "RealSense y ejecuta inspect_devices en la Jetson.");
  }
  if (candidates.size() > 1) {
    throw std::runtime_error(
      "Hay varias D435i conectadas. Especifica serial_no o usb_port_id; "
      "no se selecciona una camara al azar.");
  }
  if (candidates.front().serial.empty()) {
    throw std::runtime_error("El SDK detecto una D435i pero no pudo leer su numero de serie.");
  }
  return candidates.front();
}

}  // namespace d435i_bringup

#endif  // D435I_BRINGUP__DEVICE_SELECTION_HPP_
