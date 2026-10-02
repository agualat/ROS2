"""ROS launch orchestration; USB discovery and camera processing are C++."""

import math
import os
import re
import subprocess

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, LogInfo, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _value(context, name):
    return LaunchConfiguration(name).perform(context)


def _setup(context):
    package = "d435i_bringup"
    detector = os.path.join(get_package_prefix(package), "lib", package, "inspect_devices")
    try:
        result = subprocess.run(
            [detector, "--select", "--serial-no", _value(context, "serial_no"),
             "--usb-port-id", _value(context, "usb_port_id")],
            capture_output=True, text=True, timeout=15, check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("La consulta USB RealSense excedio 15 segundos.") from error
    except OSError as error:
        raise RuntimeError("No se pudo ejecutar inspect_devices; compila y sourcea el workspace.") from error
    if result.returncode:
        raise RuntimeError("No se inicio la D435i:\n" + result.stderr.strip())
    serial = result.stdout.strip()
    if not re.fullmatch(r"[0-9]{8,}", serial):
        raise RuntimeError("El detector no devolvio un numero de serie USB valido.")
    name = _value(context, "camera_name")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        raise ValueError("camera_name debe ser un nombre ROS valido.")
    config_file = _value(context, "config_file")
    if not os.path.isfile(config_file):
        raise ValueError("No existe el archivo de configuracion de la D435i: " + config_file)
    driver = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(
            get_package_share_directory("realsense2_camera"), "launch", "rs_launch.py")),
        launch_arguments={
            "camera_name": name,
            "camera_namespace": _value(context, "camera_namespace"),
            "device_type": "d435i",
            "serial_no": "_" + serial,
            "usb_port_id": _value(context, "usb_port_id") or "''",
            "config_file": config_file,
        }.items(),
    )
    actions = [LogInfo(msg=result.stderr.strip()),
               LogInfo(msg="Iniciando D435i por serie USB: " + serial), driver]
    mount_tf = _value(context, "publish_mount_tf").lower()
    if mount_tf not in ("true", "false"):
        raise ValueError("publish_mount_tf debe ser true o false.")
    if mount_tf == "true":
        args = []
        for axis in ("x", "y", "z", "roll", "pitch", "yaw"):
            value = _value(context, "camera_" + axis)
            if not math.isfinite(float(value)):
                raise ValueError("La transformacion de montaje debe contener valores finitos.")
            args += ["--" + axis, value]
        args += ["--frame-id", _value(context, "base_frame"), "--child-frame-id", name + "_link"]
        actions.append(Node(
            package="tf2_ros", executable="static_transform_publisher",
            name="base_to_" + name, output="screen", arguments=args,
        ))
    return actions


def generate_launch_description():
    share = get_package_share_directory("d435i_bringup")
    defaults = {
        "serial_no": "auto", "usb_port_id": "", "camera_name": "d435i",
        "camera_namespace": "camera",
        "config_file": os.path.join(share, "config", "d435i.yaml"),
        "publish_mount_tf": "true", "base_frame": "base_link",
        "camera_x": "0.0", "camera_y": "0.0", "camera_z": "0.0",
        "camera_roll": "0.0", "camera_pitch": "0.0", "camera_yaw": "0.0",
    }
    return LaunchDescription([
        DeclareLaunchArgument(name, default_value=value) for name, value in defaults.items()
    ] + [OpaqueFunction(function=_setup)])
