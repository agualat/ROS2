"""Start original LiDAR/SLAM and the D435i; the tested packages stay intact."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    lidar_defaults = {
        "lidar_ip": "192.168.0.10", "rviz": "false", "autostart": "true",
        "laser_x": "0.0", "laser_y": "0.0", "laser_z": "0.0", "laser_yaw": "0.0",
    }
    camera_defaults = {
        "serial_no": "auto", "usb_port_id": "", "camera_x": "0.0",
        "camera_y": "0.0", "camera_z": "0.0", "camera_roll": "0.0",
        "camera_pitch": "0.0", "camera_yaw": "0.0",
    }
    declarations = [
        DeclareLaunchArgument("enable_lidar", default_value="true"),
        DeclareLaunchArgument("enable_camera", default_value="true"),
    ]
    declarations += [DeclareLaunchArgument(name, default_value=value)
                     for name, value in {**lidar_defaults, **camera_defaults}.items()]
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(
            get_package_share_directory("scr_bringup"), "launch", "lidar_slam.launch.py")),
        launch_arguments={name: LaunchConfiguration(name) for name in lidar_defaults}.items(),
        condition=IfCondition(LaunchConfiguration("enable_lidar")),
    )
    camera = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(
            get_package_share_directory("d435i_bringup"), "launch", "d435i.launch.py")),
        launch_arguments={name: LaunchConfiguration(name) for name in camera_defaults}.items(),
        condition=IfCondition(LaunchConfiguration("enable_camera")),
    )
    return LaunchDescription(declarations + [lidar, camera])
