cmake_minimum_required(VERSION 3.8)

project(vesc_teleop)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(vesc_teleop_node src/vesc_teleop_node.cpp)
target_include_directories(vesc_teleop_node PRIVATE include)
ament_target_dependencies(vesc_teleop_node rclcpp sensor_msgs std_msgs)

install(TARGETS vesc_teleop_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  enable_testing()
  add_executable(test_teleop_mapping test/test_teleop_mapping.cpp)
  target_include_directories(test_teleop_mapping PRIVATE include)
  add_test(NAME teleop_mapping COMMAND test_teleop_mapping)
endif()

ament_package()
