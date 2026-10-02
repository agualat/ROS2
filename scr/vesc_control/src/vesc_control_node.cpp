#include <rclcpp/rclcpp.hpp>

#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/int32.hpp>

#include "vesc_control/vesc_interface.hpp"
#include "vesc_control/servo_controller.hpp"
#include "vesc_control/motor_controller.hpp"
#include "vesc_control/port_detection.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <stdexcept>


class VescControlNode : public rclcpp::Node
{
public:

    VescControlNode()
        : Node("vesc_control_node")
    {
        /*
         * "auto" busca la VESC por VID:PID USB, así no
         * depende de que el kernel le asigne ttyACM0.
         * También acepta una ruta fija, p. ej.
         * /dev/serial/by-id/usb-STMicroelectronics_...
         */

        const auto requested_port =
            declare_parameter<std::string>(
                "port",
                "auto");

        const auto vesc_vid =
            declare_parameter<std::string>(
                "vesc_usb_vid",
                "0483");

        const auto vesc_pid =
            declare_parameter<std::string>(
                "vesc_usb_pid",
                "5740");

        try {
            port_ =
                vesc_control::resolvePort(
                    requested_port,
                    vesc_vid,
                    vesc_pid);
        } catch (const std::exception & error) {

            RCLCPP_FATAL(
                get_logger(),
                "%s",
                error.what());

            throw;
        }

        /*
         * Frecuencia de reenvío del último comando de motor.
         *
         * El firmware de la VESC suelta el motor si no recibe
         * nada durante timeout_msec (1000 ms por defecto),
         * así que hay que refrescar el comando muy por encima
         * de ese ritmo.
         */

        keepalive_hz_ =
            declare_parameter<double>(
                "keepalive_hz",
                50.0);

        if (keepalive_hz_ < 1.0) {
            keepalive_hz_ = 1.0;
        }

        /*
         * Sentido de giro del motor.
         *
         * Va en true porque con el cableado de fases actual
         * un ERPM positivo giraba al revés. Si algún día se
         * intercambian dos fases, poner esto en false.
         */

        invert_motor_ =
            declare_parameter<bool>(
                "invert_motor",
                true);

        /*
         * Watchdog de comandos.
         *
         * Si no llega ningún /vesc/motor_erpm en este tiempo
         * se libera el motor (corriente 0). Así, si se cae
         * el nodo que manda (teleop, navegación...), el carro
         * no sigue a la última velocidad para siempre.
         *
         * Quien publique ERPM tiene que repetirlo mientras
         * quiera que el motor gire. 0 desactiva el watchdog
         * (comportamiento anterior: un comando dura hasta
         * que llegue otro).
         */

        command_timeout_ =
            declare_parameter<double>(
                "command_timeout_sec",
                0.5);

        if (!std::isfinite(command_timeout_) ||
            command_timeout_ < 0.0)
        {
            command_timeout_ = 0.5;
        }

        RCLCPP_INFO(
            get_logger(),
            "================================");

        RCLCPP_INFO(
            get_logger(),
            "VESC 6 MK VI");

        RCLCPP_INFO(
            get_logger(),
            "Puerto: %s (parametro: %s)",
            port_.c_str(),
            requested_port.c_str());

        RCLCPP_INFO(
            get_logger(),
            "================================");


        // ------------------------------------------------
        // VESC INTERFACE
        // ------------------------------------------------

        vesc_ =
            std::make_unique<
                vesc_control::VescInterface>();

        if (!vesc_->connect(port_)) {

            RCLCPP_FATAL(
                get_logger(),
                "No se pudo conectar a la VESC");

            throw std::runtime_error(
                "VESC connection failed");
        }

        RCLCPP_INFO(
            get_logger(),
            "VESC conectada");


        // ------------------------------------------------
        // SERVO
        // ------------------------------------------------

        servo_ =
            std::make_unique<
                vesc_control::ServoController>(
                    *vesc_);

        servo_sub_ =
            create_subscription<
                std_msgs::msg::Float32>(
                "/vesc/servo_cmd",
                10,
                std::bind(
                    &VescControlNode::servoCallback,
                    this,
                    std::placeholders::_1));

        RCLCPP_INFO(
            get_logger(),
            "Servo activo");

        RCLCPP_INFO(
            get_logger(),
            "Servo range: 0.1 - 0.9");


        // ------------------------------------------------
        // MOTOR TRIFÁSICO
        // ------------------------------------------------

        motor_ =
            std::make_unique<
                vesc_control::MotorController>(
                    *vesc_,
                    invert_motor_);

        motor_sub_ =
            create_subscription<
                std_msgs::msg::Int32>(
                "/vesc/motor_erpm",
                10,
                std::bind(
                    &VescControlNode::motorCallback,
                    this,
                    std::placeholders::_1));

        keepalive_timer_ =
            create_wall_timer(
                std::chrono::duration_cast<
                    std::chrono::nanoseconds>(
                        std::chrono::duration<double>(
                            1.0 / keepalive_hz_)),
                std::bind(
                    &VescControlNode::keepAliveCallback,
                    this));

        RCLCPP_INFO(
            get_logger(),
            "Motor trifasico activo");

        RCLCPP_INFO(
            get_logger(),
            "Motor ERPM range: %d - +%d",
            -vesc_control::MotorController::MAX_ERPM,
            vesc_control::MotorController::MAX_ERPM);

        RCLCPP_INFO(
            get_logger(),
            "Keepalive: %.1f Hz",
            keepalive_hz_);

        RCLCPP_INFO(
            get_logger(),
            "Sentido invertido: %s",
            invert_motor_ ? "si" : "no");

        if (command_timeout_ > 0.0) {
            RCLCPP_INFO(
                get_logger(),
                "Watchdog: motor libre tras %.2f s sin comandos",
                command_timeout_);
        } else {
            RCLCPP_WARN(
                get_logger(),
                "Watchdog desactivado (command_timeout_sec = 0)");
        }
    }


    ~VescControlNode() override
    {
        // No dejar el motor girando al cerrar el nodo.
        if (motor_) {
            motor_->stop();
        }
    }


private:

    // ====================================================
    // SERVO
    // ====================================================

    void servoCallback(
        const std_msgs::msg::Float32::SharedPtr msg)
    {
        const float requested =
            msg->data;

        const bool success =
            servo_->setPosition(
                requested);

        if (!success) {

            RCLCPP_ERROR(
                get_logger(),
                "Error enviando comando al servo");

            return;
        }

        RCLCPP_INFO(
            get_logger(),
            "Servo: %.3f",
            servo_->getPosition());
    }


    // ====================================================
    // MOTOR
    // ====================================================

    void motorCallback(
        const std_msgs::msg::Int32::SharedPtr msg)
    {
        const int32_t requested =
            msg->data;

        last_motor_cmd_ =
            std::chrono::steady_clock::now();

        const int32_t previous =
            motor_->getErpm();

        const bool success =
            motor_->setErpm(
                requested);

        if (!success) {

            RCLCPP_ERROR(
                get_logger(),
                "Error enviando ERPM al motor");

            return;
        }

        // Con el watchdog los comandos se repiten a ritmo
        // fijo; sólo se registra cuando cambia el valor.
        if (motor_->getErpm() != previous) {
            RCLCPP_INFO(
                get_logger(),
                "Motor ERPM: %d",
                motor_->getErpm());
        }
    }


    // ====================================================
    // KEEPALIVE
    // ====================================================

    void keepAliveCallback()
    {
        if (!motor_->isActive()) {
            return;
        }

        if (command_timeout_ > 0.0) {

            const double silence =
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() -
                    last_motor_cmd_).count();

            if (silence > command_timeout_) {

                RCLCPP_WARN(
                    get_logger(),
                    "Sin comandos de motor durante %.2f s: "
                    "motor libre",
                    silence);

                if (!motor_->stop()) {
                    RCLCPP_ERROR(
                        get_logger(),
                        "Error liberando el motor");
                }

                return;
            }
        }

        if (!motor_->keepAlive()) {

            RCLCPP_ERROR_THROTTLE(
                get_logger(),
                *get_clock(),
                1000,
                "Error en el keepalive del motor");
        }
    }


    // ====================================================
    // MEMBERS
    // ====================================================

    std::string port_;

    double keepalive_hz_;

    bool invert_motor_;

    double command_timeout_;

    std::chrono::steady_clock::time_point
        last_motor_cmd_;


    std::unique_ptr<
        vesc_control::VescInterface>
        vesc_;


    std::unique_ptr<
        vesc_control::ServoController>
        servo_;


    std::unique_ptr<
        vesc_control::MotorController>
        motor_;


    rclcpp::Subscription<
        std_msgs::msg::Float32>::SharedPtr
        servo_sub_;


    rclcpp::Subscription<
        std_msgs::msg::Int32>::SharedPtr
        motor_sub_;


    rclcpp::TimerBase::SharedPtr
        keepalive_timer_;
};


// ========================================================
// MAIN
// ========================================================

int main(
    int argc,
    char * argv[])
{
    rclcpp::init(argc, argv);

    auto node =
        std::make_shared<
            VescControlNode>();

    rclcpp::spin(node);

    rclcpp::shutdown();

    return 0;
}
