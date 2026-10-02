#include <rclcpp/rclcpp.hpp>

#include <sensor_msgs/msg/joy.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_msgs/msg/int32.hpp>

#include "vesc_teleop/teleop_mapping.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>


/*
 * Convierte /joy (mando Bluetooth o USB) en los comandos que
 * entiende vesc_control_node:
 *   /vesc/motor_erpm  (Int32)
 *   /vesc/servo_cmd   (Float32, 0.1 - 0.9)
 *
 * Seguridad:
 *   - El motor sólo se mueve con el botón de hombre muerto
 *     pulsado; al soltarlo se manda ERPM 0.
 *   - Si dejan de llegar mensajes /joy (mando desconectado,
 *     batería agotada, joy_node caído) se manda ERPM 0.
 */
class VescTeleopNode : public rclcpp::Node
{
public:

    VescTeleopNode()
        : Node("vesc_teleop_node")
    {
        config_.deadman_button =
            declare_parameter<int>("deadman_button", config_.deadman_button);
        config_.turbo_button =
            declare_parameter<int>("turbo_button", config_.turbo_button);
        config_.throttle_axis =
            declare_parameter<int>("throttle_axis", config_.throttle_axis);
        config_.steering_axis =
            declare_parameter<int>("steering_axis", config_.steering_axis);
        config_.max_erpm =
            declare_parameter<int>("max_erpm", config_.max_erpm);
        config_.turbo_max_erpm =
            declare_parameter<int>("turbo_max_erpm", config_.turbo_max_erpm);
        config_.invert_throttle =
            declare_parameter<bool>("invert_throttle", config_.invert_throttle);
        config_.steering_center = static_cast<float>(
            declare_parameter<double>("steering_center", config_.steering_center));
        config_.steering_range = static_cast<float>(
            declare_parameter<double>("steering_range", config_.steering_range));
        config_.invert_steering =
            declare_parameter<bool>("invert_steering", config_.invert_steering);

        joy_timeout_ =
            declare_parameter<double>("joy_timeout_sec", 0.5);

        if (config_.max_erpm < 0 || config_.turbo_max_erpm < 0 ||
            !std::isfinite(joy_timeout_) || joy_timeout_ <= 0.0)
        {
            throw std::invalid_argument(
                "max_erpm y turbo_max_erpm deben ser >= 0 y joy_timeout_sec > 0");
        }

        erpm_pub_ =
            create_publisher<std_msgs::msg::Int32>("/vesc/motor_erpm", 10);
        servo_pub_ =
            create_publisher<std_msgs::msg::Float32>("/vesc/servo_cmd", 10);

        joy_sub_ =
            create_subscription<sensor_msgs::msg::Joy>(
                "joy",
                rclcpp::SensorDataQoS(),
                std::bind(
                    &VescTeleopNode::joyCallback,
                    this,
                    std::placeholders::_1));

        watchdog_timer_ =
            create_wall_timer(
                std::chrono::milliseconds(50),
                std::bind(&VescTeleopNode::watchdogCallback, this));

        RCLCPP_INFO(
            get_logger(),
            "Teleop listo: mantener boton %d para mover, boton %d turbo. "
            "ERPM max %d (turbo %d), timeout mando %.2f s",
            config_.deadman_button,
            config_.turbo_button,
            config_.max_erpm,
            config_.turbo_max_erpm,
            joy_timeout_);
    }


    /*
     * Parar el motor al cerrar el teleop (Ctrl+C). Si no,
     * vesc_control_node seguiría reenviando el último ERPM.
     * Se hace antes del shutdown porque después ya no se
     * puede publicar.
     */
    void stopOnShutdown()
    {
        if (last_erpm_ != 0) {
            publishErpm(0);
        }
    }


private:

    void joyCallback(
        const sensor_msgs::msg::Joy::ConstSharedPtr msg)
    {
        last_joy_ = std::chrono::steady_clock::now();
        joy_seen_ = true;

        const auto command =
            vesc_teleop::computeCommand(config_, msg->axes, msg->buttons);

        // Mientras el motor gira se repite en cada /joy
        // (autorepeat 20 Hz): vesc_control_node libera el
        // motor si deja de recibir comandos (watchdog).
        // El 0 sólo se manda una vez, al cambiar.
        if (command.erpm != 0 || command.erpm != last_erpm_) {
            publishErpm(command.erpm);
        }

        if (std::fabs(command.servo - last_servo_) > 1e-3f) {
            std_msgs::msg::Float32 servo;
            servo.data = command.servo;
            servo_pub_->publish(servo);
            last_servo_ = command.servo;
        }
    }


    void watchdogCallback()
    {
        if (!joy_seen_ || last_erpm_ == 0) {
            return;
        }

        const double silence = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - last_joy_).count();

        if (silence > joy_timeout_) {
            RCLCPP_WARN(
                get_logger(),
                "Sin mensajes del mando durante %.2f s: motor a 0",
                silence);
            publishErpm(0);
        }
    }


    void publishErpm(int32_t erpm)
    {
        std_msgs::msg::Int32 msg;
        msg.data = erpm;
        erpm_pub_->publish(msg);
        last_erpm_ = erpm;
    }


    vesc_teleop::TeleopConfig config_;

    double joy_timeout_{};

    std::chrono::steady_clock::time_point last_joy_;
    bool joy_seen_{false};

    int32_t last_erpm_{0};
    float last_servo_{-1.0f};

    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr erpm_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr servo_pub_;
    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr joy_sub_;
    rclcpp::TimerBase::SharedPtr watchdog_timer_;
};


int main(
    int argc,
    char * argv[])
{
    rclcpp::init(argc, argv);

    try {
        auto node = std::make_shared<VescTeleopNode>();

        rclcpp::contexts::get_global_default_context()
            ->add_pre_shutdown_callback(
                [node]() { node->stopOnShutdown(); });

        rclcpp::spin(node);
    } catch (const std::exception & error) {
        RCLCPP_FATAL(
            rclcpp::get_logger("vesc_teleop_node"),
            "%s",
            error.what());
        rclcpp::shutdown();
        return 1;
    }

    rclcpp::shutdown();
    return 0;
}
