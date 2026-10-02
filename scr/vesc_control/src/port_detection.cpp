#include "vesc_control/port_detection.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace vesc_control
{

namespace
{

std::string readAttribute(
    const fs::path & path)
{
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    return value;
}


std::string toLower(
    std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return text;
}


/*
 * Sube desde /sys/class/tty/<nombre>/device hasta el
 * dispositivo USB, que es el que tiene idVendor/idProduct.
 */
fs::path usbParent(
    const fs::path & path)
{
    std::error_code error;
    auto parent = fs::canonical(path, error);

    while (!error &&
        !parent.empty() &&
        parent != parent.root_path())
    {
        if (fs::exists(parent / "idVendor", error) &&
            fs::exists(parent / "idProduct", error))
        {
            return parent;
        }
        parent = parent.parent_path();
    }

    return {};
}

}  // namespace


std::vector<SerialPortInfo> listSerialPorts()
{
    std::vector<SerialPortInfo> ports;

    std::error_code error;
    fs::directory_iterator cursor("/sys/class/tty", error);
    const fs::directory_iterator end;

    for (; !error && cursor != end; cursor.increment(error)) {

        const auto name =
            cursor->path().filename().string();

        if (name.rfind("ttyACM", 0) != 0 &&
            name.rfind("ttyUSB", 0) != 0)
        {
            continue;
        }

        SerialPortInfo info;
        info.device = "/dev/" + name;

        const auto usb =
            usbParent(cursor->path() / "device");

        if (!usb.empty()) {
            info.vendor_id = readAttribute(usb / "idVendor");
            info.product_id = readAttribute(usb / "idProduct");
            info.product = readAttribute(usb / "product");
        }

        ports.push_back(info);
    }

    std::sort(
        ports.begin(),
        ports.end(),
        [](const SerialPortInfo & a, const SerialPortInfo & b) {
            return a.device < b.device;
        });

    return ports;
}


std::string resolvePort(
    const std::string & requested,
    const std::string & vendor_id,
    const std::string & product_id)
{
    if (!requested.empty() && requested != "auto") {
        return requested;
    }

    const auto ports = listSerialPorts();

    std::vector<std::string> matches;
    std::string seen;

    for (const auto & port : ports) {

        seen += "\n  " + port.device + " (" +
            (port.vendor_id.empty()
                ? std::string("sin datos USB")
                : port.vendor_id + ":" + port.product_id +
                    " " + port.product) +
            ")";

        if (toLower(port.vendor_id) == toLower(vendor_id) &&
            toLower(port.product_id) == toLower(product_id))
        {
            matches.push_back(port.device);
        }
    }

    if (seen.empty()) {
        seen = "\n  ninguno";
    }

    if (matches.empty()) {
        throw std::runtime_error(
            "No se encontro la VESC (USB " + vendor_id + ":" +
            product_id + "). Puertos serie detectados:" + seen);
    }

    if (matches.size() > 1) {
        throw std::runtime_error(
            "Hay varios puertos con USB " + vendor_id + ":" +
            product_id + "; indica el parametro port "
            "(por ejemplo /dev/serial/by-id/...). Puertos:" + seen);
    }

    return matches.front();
}

}  // namespace vesc_control
