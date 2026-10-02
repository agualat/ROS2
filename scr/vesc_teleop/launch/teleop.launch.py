"""Mando (joy) + teleop + opcionalmente el nodo de la VESC."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory("vesc_teleop"), "config", "teleop.yaml")

    return LaunchDescription([
        DeclareLaunchArgument("config_file", default_value=config),
        DeclareLaunchArgument(
            "start_vesc", default_value="true",
            description="Arrancar tambien vesc_control_node"),
        DeclareLaunchArgument(
            "vesc_port", default_value="auto",
            description="auto (busca la VESC por USB) o una ruta /dev/..."),
        Node(
            package="joy", executable="game_controller_node",
            name="game_controller_node", output="screen",
            parameters=[LaunchConfiguration("config_file")],
        ),
        Node(
            package="vesc_teleop", executable="vesc_teleop_node",
            name="vesc_teleop_node", output="screen",
            parameters=[LaunchConfiguration("config_file")],
        ),
        Node(
            package="vesc_control", executable="vesc_control_node",
            name="vesc_control_node", output="screen",
            parameters=[{"port": LaunchConfiguration("vesc_port")}],
            condition=IfCondition(LaunchConfiguration("start_vesc")),
        ),
    ])
