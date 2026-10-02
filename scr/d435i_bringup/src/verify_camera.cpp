#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include "d435i_bringup/stream_stats.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

class VerifyCamera : public rclcpp::Node
{
public:
  VerifyCamera()
  : Node("verify_d435i"), started_(std::chrono::steady_clock::now())
  {
    const auto prefix = declare_parameter<std::string>("topic_prefix", "/camera/d435i");
    timeout_ = declare_parameter<double>("timeout_sec", 20.0);
    measurement_ = declare_parameter<double>("measurement_sec", 5.0);
    max_silence_ = declare_parameter<double>("max_silence_sec", 1.0);
    samples_ = declare_parameter<int>("min_samples", 3);
    if (!std::isfinite(timeout_) || !std::isfinite(measurement_) || measurement_ <= 0 ||
      !std::isfinite(max_silence_) || max_silence_ <= 0 ||
      timeout_ <= measurement_ || samples_ < 1 || prefix.empty())
    {
      throw std::invalid_argument("Requiere timeout_sec > measurement_sec > 0, max_silence_sec > 0, min_samples > 0 y topic_prefix");
    }
    const auto qos = rclcpp::SensorDataQoS().keep_last(1);
    color_ = create_subscription<sensor_msgs::msg::Image>(
      prefix + "/color/image_raw", qos,
      [this](const sensor_msgs::msg::Image::ConstSharedPtr message) {
        if (valid_image(*message)) { observe(color_stats_, message->header.stamp); }
      });
    depth_ = create_subscription<sensor_msgs::msg::Image>(
      prefix + "/depth/image_rect_raw", qos,
      [this](const sensor_msgs::msg::Image::ConstSharedPtr message) {
        if (valid_image(*message)) { observe(depth_stats_, message->header.stamp); }
      });
    imu_ = create_subscription<sensor_msgs::msg::Imu>(
      prefix + "/imu", qos,
      [this](const sensor_msgs::msg::Imu::ConstSharedPtr message) {
        const auto & a = message->linear_acceleration;
        const auto & g = message->angular_velocity;
        if (!message->header.frame_id.empty() &&
          std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
          std::isfinite(g.x) && std::isfinite(g.y) && std::isfinite(g.z))
        {
          observe(imu_stats_, message->header.stamp);
        }
      });
    RCLCPP_INFO(get_logger(), "Comprobando RGB, profundidad e IMU bajo %s", prefix.c_str());
    timer_ = create_wall_timer(std::chrono::milliseconds(100), [this]() { check(); });
  }

  int result() const { return result_; }

private:
  static bool valid_image(const sensor_msgs::msg::Image & message)
  {
    return message.width > 0 && message.height > 0 && message.step > 0 &&
           !message.header.frame_id.empty() &&
           message.data.size() >= static_cast<uint64_t>(message.step) * message.height;
  }

  void observe(d435i_bringup::StreamStats & stats, const builtin_interfaces::msg::Time & stamp)
  {
    const double receive = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started_).count();
    if (!received_first_) {
      received_first_ = true;
      first_sample_time_ = receive;
    }
    const double age = (now() - rclcpp::Time(stamp, get_clock()->get_clock_type())).seconds();
    stats.observe(receive, age);
  }

  void report(const char * name, const d435i_bringup::StreamStats & stats)
  {
    RCLCPP_INFO(get_logger(), "%s: %llu muestras, %.1f Hz, intervalo maximo=%.1f ms",
      name, static_cast<unsigned long long>(stats.count), stats.hz(), stats.max_gap_ms);
    if (stats.valid_ages) {
      RCLCPP_INFO(get_logger(), "%s: antiguedad media=%.1f ms maxima=%.1f ms (header ROS)",
        name, stats.mean_age_ms(), stats.max_age_ms);
    }
    if (stats.valid_ages != stats.count) {
      RCLCPP_WARN(get_logger(), "%s: timestamps no comparables; revisar sincronizacion de relojes", name);
    }
  }

  void check()
  {
    const bool received = color_stats_.count >= static_cast<uint64_t>(samples_) &&
      depth_stats_.count >= static_cast<uint64_t>(samples_) &&
      imu_stats_.count >= static_cast<uint64_t>(samples_);
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started_).count();
    const bool active = color_stats_.active(elapsed, max_silence_) &&
      depth_stats_.active(elapsed, max_silence_) && imu_stats_.active(elapsed, max_silence_);
    const bool complete = received && active && elapsed - first_sample_time_ >= measurement_;
    if (!complete && elapsed < timeout_) { return; }
    result_ = complete ? 0 : 2;
    report("RGB", color_stats_);
    report("Profundidad", depth_stats_);
    report("IMU", imu_stats_);
    if (complete) {
      RCLCPP_INFO(get_logger(), "D435i: se recibieron los tres flujos validos.");
    } else {
      RCLCPP_ERROR(get_logger(), "Timeout: faltan flujos o se interrumpieron; comprueba driver, USB y topicos.");
    }
    timer_->cancel();
    rclcpp::shutdown();
  }

  std::chrono::steady_clock::time_point started_;
  double timeout_{};
  double measurement_{};
  double max_silence_{};
  double first_sample_time_{};
  bool received_first_{};
  int samples_{};
  d435i_bringup::StreamStats color_stats_, depth_stats_, imu_stats_;
  int result_{2};
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr color_, depth_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    const auto node = std::make_shared<VerifyCamera>();
    rclcpp::spin(node);
    const int result = node->result();
    if (rclcpp::ok()) { rclcpp::shutdown(); }
    return result;
  } catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    if (rclcpp::ok()) { rclcpp::shutdown(); }
    return 2;
  }
}
