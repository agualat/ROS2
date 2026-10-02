#include "d435i_bringup/device_selection.hpp"

#include <librealsense2/rs.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string read_attribute(const fs::path & path)
{
  std::ifstream input(path);
  std::string value;
  std::getline(input, value);
  return value;
}

std::vector<fs::path> entries(const fs::path & root)
{
  std::vector<fs::path> result;
  std::error_code error;
  fs::directory_iterator cursor(root, error);
  const fs::directory_iterator end;
  while (!error && cursor != end) {
    result.push_back(cursor->path());
    cursor.increment(error);
  }
  std::sort(result.begin(), result.end());
  return result;
}

fs::path usb_parent(const fs::path & path)
{
  std::error_code error;
  auto parent = fs::canonical(path, error);
  while (!error && !parent.empty() && parent != parent.root_path()) {
    if (fs::exists(parent / "idVendor", error) && fs::exists(parent / "idProduct", error)) {
      return parent;
    }
    parent = parent.parent_path();
  }
  return {};
}

void list_serial_ports(std::ostream & output)
{
  output << "Puertos serie Linux (equivalentes a COM):\n";
  bool found = false;
  for (const auto & port : entries("/dev")) {
    const auto name = port.filename().string();
    if (name.rfind("ttyACM", 0) != 0 && name.rfind("ttyUSB", 0) != 0) {
      continue;
    }
    found = true;
    output << "  " << port.string();
    const auto parent = usb_parent(fs::path("/sys/class/tty") / name / "device");
    if (!parent.empty()) {
      output << " | " << read_attribute(parent / "product")
             << " | USB " << read_attribute(parent / "idVendor") << ":"
             << read_attribute(parent / "idProduct");
    }
    output << '\n';
    for (const auto & alias : entries("/dev/serial/by-id")) {
      std::error_code error;
      if (fs::equivalent(alias, port, error) && !error) {
        output << "    ruta estable: " << alias.string() << '\n';
      }
    }
  }
  if (!found) {
    output << "  Ninguno detectado.\n";
  }
}

std::string sdk_info(const rs2::device & device, rs2_camera_info field)
{
  return device.supports(field) ? device.get_info(field) : "";
}

std::vector<d435i_bringup::Device> cameras()
{
  rs2::context context;
  std::vector<d435i_bringup::Device> result;
  for (const auto & device : context.query_devices()) {
    d435i_bringup::Device info;
    info.name = sdk_info(device, RS2_CAMERA_INFO_NAME);
    if (!d435i_bringup::is_d435i(info.name)) {
      continue;
    }
    info.serial = sdk_info(device, RS2_CAMERA_INFO_SERIAL_NUMBER);
    info.physical_port = sdk_info(device, RS2_CAMERA_INFO_PHYSICAL_PORT);
    info.usb_type = sdk_info(device, RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
    info.usb_port = d435i_bringup::usb_port_from_path(info.physical_port);
    // The native kernel and RSUSB backends report physical paths differently.
    // Linux sysfs gives the same topology identifier for either SDK backend.
    for (const auto & usb : entries("/sys/bus/usb/devices")) {
      if (!info.serial.empty() && read_attribute(usb / "idVendor") == "8086" &&
        read_attribute(usb / "serial") == info.serial)
      {
        info.usb_port = usb.filename().string();
        break;
      }
    }
    result.push_back(info);
  }
  return result;
}

void list_cameras(const std::vector<d435i_bringup::Device> & devices, std::ostream & output)
{
  output << "RealSense D435i: camara USB, sin asignacion de puerto serie.\n";
  for (const auto & device : devices) {
    output << "  " << device.name << " | serie=" << device.serial
           << " | USB=" << device.usb_port << " | tipo USB=" << device.usb_type << '\n';
    if (device.usb_type.rfind("2", 0) == 0) {
      output << "  Conexion USB 2: usar USB 3 para los perfiles RGB 60 / profundidad 90 Hz.\n";
    }
  }
  if (devices.empty()) {
    output << "  Ninguna D435i detectada por el SDK.\n";
  }
}

}  // namespace

int main(int argc, char ** argv)
{
  bool select = false;
  std::string serial = "auto";
  std::string port;
  try {
    for (int index = 1; index < argc; ++index) {
      const std::string argument(argv[index]);
      if (argument == "--select") {
        select = true;
      } else if ((argument == "--serial-no" || argument == "--usb-port-id") && index + 1 < argc) {
        (argument == "--serial-no" ? serial : port) = argv[++index];
      } else if (argument == "--help") {
        std::cout << "inspect_devices [--select] [--serial-no auto|SERIE] [--usb-port-id 2-1.3]\n";
        return 0;
      } else {
        throw std::runtime_error("Argumento desconocido o sin valor: " + argument);
      }
    }
    d435i_bringup::normalize_serial(serial);
    auto & diagnostics = select ? std::cerr : std::cout;
    list_serial_ports(diagnostics);
    const auto devices = cameras();
    list_cameras(devices, diagnostics);
    if (select) {
      const auto selected = d435i_bringup::select_device(devices, serial, port);
      // Machine-readable stdout for the launch; diagnostics go to stderr.
      std::cout << selected.serial << '\n';
    }
    return 0;
  } catch (const rs2::error & error) {
    std::cerr << "Error del SDK RealSense: " << error.what() << '\n';
  } catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
  }
  return 2;
}
