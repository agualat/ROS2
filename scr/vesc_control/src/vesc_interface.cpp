#include "vesc_control/vesc_interface.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace vesc_control
{

VescInterface::VescInterface()
    : fd_(-1),
      connected_(false)
{
}

VescInterface::~VescInterface()
{
    disconnect();
}


bool VescInterface::connect(
    const std::string & port)
{
    fd_ = open(
        port.c_str(),
        O_RDWR | O_NOCTTY | O_SYNC);

    if (fd_ < 0) {
        return false;
    }

    struct termios tty{};

    if (tcgetattr(fd_, &tty) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag =
        (tty.c_cflag & ~CSIZE) | CS8;

    tty.c_cflag |= CLOCAL | CREAD;

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    tcflush(fd_, TCIOFLUSH);

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    connected_ = true;

    return true;
}


void VescInterface::disconnect()
{
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    connected_ = false;
}


bool VescInterface::isConnected() const
{
    return connected_;
}


uint16_t VescInterface::crc16(
    const uint8_t * data,
    size_t length) const
{
    uint16_t crc = 0;

    for (size_t i = 0; i < length; ++i) {

        crc ^= static_cast<uint16_t>(data[i]) << 8;

        for (int j = 0; j < 8; ++j) {

            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}


bool VescInterface::sendPacket(
    const uint8_t * payload,
    size_t length)
{
    if (!connected_ || length > 255) {
        return false;
    }

    uint8_t packet[260];

    size_t index = 0;

    /*
     * VESC short packet:
     *
     * 0x02
     * length
     * payload
     * CRC high
     * CRC low
     * 0x03
     */

    packet[index++] = 2;

    packet[index++] =
        static_cast<uint8_t>(length);

    for (size_t i = 0; i < length; ++i) {
        packet[index++] = payload[i];
    }

    uint16_t crc =
        crc16(payload, length);

    packet[index++] =
        static_cast<uint8_t>(crc >> 8);

    packet[index++] =
        static_cast<uint8_t>(crc & 0xFF);

    packet[index++] = 3;

    ssize_t written =
        write(
            fd_,
            packet,
            index);

    return written ==
        static_cast<ssize_t>(index);
}


/*
 * ---------------------------------------------------------
 * SERVO
 * ---------------------------------------------------------
 *
 * COMM_SET_SERVO_POS = 12
 *
 * VESC espera:
 *
 *     float16(position * 1000)
 *
 * Por ejemplo:
 *
 *     0.1 -> 100
 *     0.5 -> 500
 *     0.9 -> 900
 *
 * Se envía como int16 big-endian.
 */

bool VescInterface::setServoPosition(
    float position)
{
    if (!connected_) {
        return false;
    }

    position =
        std::clamp(
            position,
            0.1f,
            0.9f);

    constexpr uint8_t COMM_SET_SERVO_POS = 12;

    int16_t value =
        static_cast<int16_t>(
            position * 1000.0f);

    uint8_t payload[3];

    payload[0] =
        COMM_SET_SERVO_POS;

    payload[1] =
        static_cast<uint8_t>(
            (value >> 8) & 0xFF);

    payload[2] =
        static_cast<uint8_t>(
            value & 0xFF);

    return sendPacket(
        payload,
        sizeof(payload));
}


/*
 * ---------------------------------------------------------
 * MOTOR TRIFÁSICO
 * ---------------------------------------------------------
 *
 * COMM_SET_RPM = 8
 *
 * ERPM es un valor signed de 32 bits.
 */

bool VescInterface::setErpm(
    int32_t erpm)
{
    if (!connected_) {
        return false;
    }

    constexpr uint8_t COMM_SET_RPM = 8;

    uint8_t payload[5];

    payload[0] =
        COMM_SET_RPM;

    payload[1] =
        static_cast<uint8_t>(
            (erpm >> 24) & 0xFF);

    payload[2] =
        static_cast<uint8_t>(
            (erpm >> 16) & 0xFF);

    payload[3] =
        static_cast<uint8_t>(
            (erpm >> 8) & 0xFF);

    payload[4] =
        static_cast<uint8_t>(
            erpm & 0xFF);

    return sendPacket(
        payload,
        sizeof(payload));
}


/*
 * ---------------------------------------------------------
 * CORRIENTE
 * ---------------------------------------------------------
 *
 * COMM_SET_CURRENT = 6
 *
 * Se envía int32 = amperios * 1000.
 *
 * Con 0 A la VESC libera el motor (rueda libre).
 * Se usa para parar de forma explícita, sin depender
 * del timeout interno del firmware.
 */

bool VescInterface::setCurrent(
    float amps)
{
    if (!connected_) {
        return false;
    }

    constexpr uint8_t COMM_SET_CURRENT = 6;

    const int32_t value =
        static_cast<int32_t>(
            amps * 1000.0f);

    uint8_t payload[5];

    payload[0] =
        COMM_SET_CURRENT;

    payload[1] =
        static_cast<uint8_t>(
            (value >> 24) & 0xFF);

    payload[2] =
        static_cast<uint8_t>(
            (value >> 16) & 0xFF);

    payload[3] =
        static_cast<uint8_t>(
            (value >> 8) & 0xFF);

    payload[4] =
        static_cast<uint8_t>(
            value & 0xFF);

    return sendPacket(
        payload,
        sizeof(payload));
}

}  // namespace vesc_control