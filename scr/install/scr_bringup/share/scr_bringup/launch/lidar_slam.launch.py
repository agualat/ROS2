"""
Arranca el LiDAR Hokuyo UST-10LX, el arbol de TF y SLAM.

    ros2 launch scr_bringup lidar_slam.launch.py

Mapeo sin odometria: el movimiento se deduce comparando
scans consecutivos. Mueve el robot despacio.
"""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    EmitEvent,
    LogInfo,
    RegisterEventHandler,
)
from launch.conditions import IfCondition
from launch.events import matches_action
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import LifecycleNode, Node
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState

from lifecycle_msgs.msg import Transition


def generate_launch_description():

    share = get_package_share_directory("scr_bringup")

    config = os.path.join(share, "config", "slam_toolbox.yaml")
    rviz_config = os.path.join(share, "config", "slam.rviz")

    autostart = LaunchConfiguration("autostart")

    # ----------------------------------------------------
    # Argumentos
    # ----------------------------------------------------

    args = [
        DeclareLaunchArgument(
            "lidar_ip",
            default_value="192.168.0.10",
            description="IP del Hokuyo."),

        # Por defecto no arranca RViz: si lanzas esto por SSH
        # sin DISPLAY apuntado al monitor de la Jetson, RViz
        # no abre y se lleva por delante el resto del launch.
        #
        #     export DISPLAY=:1
        #     ros2 launch scr_bringup lidar_slam.launch.py rviz:=true
        DeclareLaunchArgument(
            "rviz",
            default_value="false",
            description="Abre RViz con la vista de SLAM ya montada."),

        DeclareLaunchArgument(
            "autostart",
            default_value="true",
            description="Configura y activa slam_toolbox solo. "
                        "En false hay que hacerlo a mano con "
                        "'ros2 lifecycle set'."),

        # Posicion del LiDAR respecto al centro del robot,
        # en metros. Midelo en el coche y ajustalo: si esto
        # esta mal, el mapa sale girado o desplazado.
        DeclareLaunchArgument(
            "laser_x",
            default_value="0.0",
            description="Adelante (+) / atras (-) desde base_link."),

        DeclareLaunchArgument(
            "laser_y",
            default_value="0.0",
            description="Izquierda (+) / derecha (-) desde base_link."),

        DeclareLaunchArgument(
            "laser_z",
            default_value="0.0",
            description="Altura sobre base_link."),

        DeclareLaunchArgument(
            "laser_yaw",
            default_value="0.0",
            description="Giro del LiDAR en radianes. Si esta "
                        "montado mirando atras, pon 3.14159."),
    ]

    # ----------------------------------------------------
    # LiDAR
    # ----------------------------------------------------
    #
    # El Hokuyo admite UNA sola conexion TCP. Si ya tienes
    # un urg_node corriendo a mano, este falla con
    # "could not open ethernet port": cierra el otro antes.

    urg = Node(
        package="urg_node",
        executable="urg_node_driver",
        name="urg_node",
        output="screen",
        parameters=[{
            "ip_address": LaunchConfiguration("lidar_ip"),
            "ip_port": 10940,
            "laser_frame_id": "laser",
            "angle_min": -2.356194,
            "angle_max": 2.356194,
        }])

    # ----------------------------------------------------
    # TF: donde esta montado el LiDAR
    # ----------------------------------------------------

    base_to_laser = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="base_to_laser",
        output="screen",
        arguments=[
            "--x", LaunchConfiguration("laser_x"),
            "--y", LaunchConfiguration("laser_y"),
            "--z", LaunchConfiguration("laser_z"),
            "--yaw", LaunchConfiguration("laser_yaw"),
            "--roll", "0.0",
            "--pitch", "0.0",
            "--frame-id", "base_link",
            "--child-frame-id", "laser",
        ])

    # ----------------------------------------------------
    # TF: odometria falsa
    # ----------------------------------------------------
    #
    # Transformada fija a proposito. slam_toolbox exige que
    # exista odom -> base_link, pero como no hay odometria
    # real se deja en identidad y el emparejamiento de scans
    # se encarga de todo.
    #
    # Cuando la VESC publique odometria de verdad, esto se
    # borra y se pone el nodo de odometria en su lugar.

    odom_to_base = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="odom_to_base_fake",
        output="screen",
        arguments=[
            "--x", "0.0",
            "--y", "0.0",
            "--z", "0.0",
            "--yaw", "0.0",
            "--roll", "0.0",
            "--pitch", "0.0",
            "--frame-id", "odom",
            "--child-frame-id", "base_link",
        ])

    # ----------------------------------------------------
    # SLAM
    # ----------------------------------------------------
    #
    # sync y no async: sin odometria conviene procesar todos
    # los scans en orden. El async descarta scans cuando va
    # justo de CPU, y cada scan perdido es mas distancia
    # entre los dos que se comparan, o sea peor encaje.
    #
    # En Jazzy es un nodo de ciclo de vida: arranca en
    # 'unconfigured' y no publica nada hasta que se le
    # ordenan las transiciones de abajo.

    slam = LifecycleNode(
        package="slam_toolbox",
        executable="sync_slam_toolbox_node",
        name="slam_toolbox",
        namespace="",
        output="screen",
        parameters=[
            config,
            {
                "use_lifecycle_manager": False,
                "use_sim_time": False,
            },
        ])

    # unconfigured -> inactive
    configure = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(slam),
            transition_id=Transition.TRANSITION_CONFIGURE),
        condition=IfCondition(autostart))

    # inactive -> active, en cuanto termine de configurarse
    activate = RegisterEventHandler(
        OnStateTransition(
            target_lifecycle_node=slam,
            start_state="configuring",
            goal_state="inactive",
            entities=[
                LogInfo(msg="[scr_bringup] Activando slam_toolbox."),
                EmitEvent(
                    event=ChangeState(
                        lifecycle_node_matcher=matches_action(slam),
                        transition_id=Transition.TRANSITION_ACTIVATE)),
            ]),
        condition=IfCondition(autostart))

    # ----------------------------------------------------
    # RViz
    # ----------------------------------------------------

    rviz = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        output="screen",
        arguments=["-d", rviz_config],
        condition=IfCondition(LaunchConfiguration("rviz")))

    return LaunchDescription(
        args + [
            urg,
            base_to_laser,
            odom_to_base,
            slam,
            configure,
            activate,
            rviz,
        ])
