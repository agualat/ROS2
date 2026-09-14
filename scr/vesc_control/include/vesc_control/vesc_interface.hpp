#ifndef VESC_CONTROL__VESC_INTERFACE_HPP_
#define VESC_CONTROL__VESC_INTERFACE_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

namespace vesc_control
{

class VescInterface
{
public:
    VescInterface();
    ~VescInterface();

    bool connect(const std::string & port);
    void disconnect();

    bool isConnected() const;

    bool setServoPosition(float position);

    bool setErpm(int32_t erpm);

    bool setCurrent(float amps);

private:
    int fd_;
    bool connected_;

    uint16_t crc16(
        const uint8_t * data,
        size_t length) const;

    bool sendPacket(
        const uint8_t * payload,
        size_t length);
};

}  // namespace vesc_control

#endif