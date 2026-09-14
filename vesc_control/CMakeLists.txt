cmake_minimum_required(VERSION 3.8)

project(vesc_control)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
  )
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)


include_directories(
  include
)


add_executable(
  vesc_control_node

  src/vesc_control_node.cpp
  src/vesc_interface.cpp
  src/servo_controller.cpp
  src/motor_controller.cpp
)


ament_target_dependencies(
  vesc_control_node

  rclcpp
  std_msgs
)


install(
  TARGETS
    vesc_control_node

  DESTINATION
    lib/${PROJECT_NAME}
)


install(
  DIRECTORY
    include/

  DESTINATION
    include
)


ament_package()
