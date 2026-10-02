#include "d435i_bringup/device_selection.hpp"

#include <functional>
#include <iostream>
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
void rejects(const std::function<void()> & operation, const std::string & message)
{
  bool rejected = false;
  try { operation(); } catch (const std::runtime_error &) { rejected = true; }
  require(rejected, message);
}
}  // namespace

int main()
{
  using namespace d435i_bringup;
  try {
    const Device one{"Intel RealSense D435I", "123456789012", "2-1.3", "", "3.2"};
    const Device two{"Intel RealSense D435i", "987654321098", "2-2", "", "3.2"};
    const Device other{"Intel RealSense D435", "111111111111", "2-3", "", "3.2"};
    require(is_d435i(one.name), "D435I should match case-insensitively");
    require(!is_d435i(other.name), "D435 without IMU must not match");
    require(!is_d435i("D435if"), "Other models must not match D435i");
    require(select_device({one}).serial == one.serial, "Single camera selection");
    require(select_device({one, other}).serial == one.serial, "Exclude other models");
    rejects([]() { select_device({}); }, "Missing camera must fail");
    rejects([&]() { select_device({one, two}); }, "Multiple cameras must fail");
    require(select_device({one, two}, two.serial).serial == two.serial, "Explicit serial");
    require(select_device({one, two}, "_" + two.serial).serial == two.serial, "ROS serial prefix");
    require(select_device({one, two}, "auto", one.usb_port).serial == one.serial, "USB topology");
    rejects([&]() { select_device({one, two}, one.serial, two.usb_port); }, "Conflicting selectors");
    rejects([&]() { select_device({one}, "222222222222"); }, "Missing explicit serial");
    rejects([&]() { select_device({one}, "K38179-111"); }, "Model label is not USB serial");
    rejects([&]() { select_device({one}, "COM3"); }, "COM must not identify camera");
    rejects([]() { select_device({{"Intel RealSense D435I", "", "2-1", "", ""}}); },
      "No serial means binding is unreliable");
    require(usb_port_from_path("/sys/devices/usb2/2-1.3/2-1.3:1.0/video4linux/video0") == "2-1.3",
      "Linux native physical port");
    require(usb_port_from_path("2-2") == "2-2", "Plain USB identifier");
    require(usb_port_from_path("/sys/devices/usb2/2-1/2-1.3/2-1.3:1.0/video4linux/video0") == "2-1.3",
      "Select the camera port rather than the parent USB hub");
    require(usb_port_from_path("COM3").empty(), "COM must not become USB identifier");
    std::cout << checks << " device-selection checks passed\n";
    return 0;
  } catch (const std::exception & error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}
