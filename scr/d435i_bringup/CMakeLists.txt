cmake_minimum_required(VERSION 3.8)
project(d435i_bringup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(realsense2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(inspect_devices src/inspect_devices.cpp)
target_include_directories(inspect_devices PRIVATE include)
target_link_libraries(inspect_devices realsense2::realsense2)

add_executable(verify_camera src/verify_camera.cpp)
target_include_directories(verify_camera PRIVATE include)
ament_target_dependencies(verify_camera rclcpp sensor_msgs)

install(TARGETS inspect_devices verify_camera DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  enable_testing()
  add_executable(test_device_selection test/test_device_selection.cpp)
  target_include_directories(test_device_selection PRIVATE include)
  add_test(NAME device_selection COMMAND test_device_selection)
  add_executable(test_stream_stats test/test_stream_stats.cpp)
  target_include_directories(test_stream_stats PRIVATE include)
  add_test(NAME stream_stats COMMAND test_stream_stats)
endif()

ament_package()
