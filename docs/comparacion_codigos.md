# Comparacion del codigo y alcance de la rama D435i

Fecha: 2026-10-02.

Referencia probada: `VehiculoAuto/codigoroboraceragualat/ROS2/scr`.
Version modificada anteriormente: `VehiculoAuto/CodigoRoboracer/ROS2/src`.
Destino de esta nueva integracion: solo el clon `codigoroboraceragualat/ROS2`,
en la rama local `codex/d435i-jazzy`.

Se verificaron los 15 archivos originales de los dos paquetes: la referencia
coincide con el respaldo previo a las modificaciones anteriores, ignorando
solo CRLF frente a LF. La version anterior era una adaptacion de ese mismo
codigo, no una implementacion diferente del protocolo VESC.

## Diferencias de la version anterior frente al clon que funciona

| Elemento | Clon probado | Modificado anteriormente |
| --- | --- | --- |
| Workspace | Paquetes en `scr/` | Paquetes movidos a `src/` |
| Motor | ERPM limitado a +/-20000, inversion configurable, corriente cero al soltar | Misma implementacion de `motor_controller.cpp` y su cabecera |
| Servo | Rango 0.1-0.9, sin inversion del servo | Inversion opcional y rechazo de NaN |
| Comandos de motor | Reenvio a 50 Hz del ultimo comando mientras este activo | Watchdog adicional: corriente cero tras 500 ms sin nuevo comando |
| Serial VESC | Una llamada a `write` por paquete | Reintentos de escritura parcial/interrumpida y `tcdrain` |
| Bytes VESC | Comandos 8 (ERPM), 12 (servo), 6 (corriente); CRC16 | Mismos comandos y formato, conversion explicita a uint32 para ERPM negativos |
| Topics VESC | Absolutos `/vesc/...` | Relativos `vesc/...`; misma ruta en namespace raiz |
| LiDAR/SLAM | Launch y RViz originales | Launch y RViz identicos; `scan_topic` cambiado de `/scan` a `scan` |
| Arranque VESC | `ros2 run` y parametros del nodo | Launch y YAML adicionales |
| Metadatos | Dependencias originales | Algunas dependencias de launch anadidas |

El watchdog anterior cambia el comportamiento de comandos publicados una
sola vez. No se traslada a esta rama. Corriente cero libera el par motor
(rueda libre); no equivale a un frenado activo.

## Evidencia del entorno de referencia

- `scr/.vscode/c_cpp_properties.json`: ROS2 Jazzy, `/opt/ros/jazzy/include`,
  C++17 y `linux-gcc-arm64`.
- `scr/build/vesc_control/CMakeFiles/3.28.3/CMakeSystem.cmake`:
  `CMAKE_SYSTEM_PROCESSOR` y host `aarch64`.
- Los archivos `colcon_build.rc` de los dos paquetes contienen `0`.
- Ubuntu 24.04 de NVIDIA es el entorno indicado por el propietario.
- Estas evidencias describen el entorno en que se genero el codigo; la
  Jetson esta apagada y no se realizo una prueba remota.

## Cambios de esta rama

Se anade `d435i_bringup`, paquete `ament_cmake` con tres funciones:

1. `inspect_devices`: C++ / librealsense2; enumera puertos TTY sin abrirlos,
   identifica D435i por el SDK y selecciona por serie o topologia USB.
2. Driver C++ oficial `realsense2_camera`: RGB, profundidad, IMU y TF internos.
   El launch realiza la seleccion mediante el ejecutable C++ antes de arrancar.
3. `verify_camera`: C++ / rclcpp; comprueba flujos y mide frecuencia, huecos
   de recepcion y antiguedad de timestamps ROS.

El launch combinado incluye el `lidar_slam.launch.py` original. No modifica
los archivos existentes de VESC o SLAM ni sus parametros. Conserva `scr/`.
Tampoco elimina las compilaciones existentes, las saca del indice Git o
modifica firmware. `.gitignore` evita nuevos archivos generados, pero no
oculta los generados que ya estan seguidos por Git.

La arquitectura sigue en etapa de adquisicion de sensores: LiDAR para SLAM y
mapa, camara/IMU disponibles para la IA futura, VESC y motores con control
inicial. No hay IA entrenada, odometria medida o controlador de trayectoria.
La TF estatica `odom -> base_link` del launch original sigue siendo provisional.
La integracion no conecta una decision de IA a los topics de motor/servo.

## Conservacion del codigo

Antes de cambiar de rama se guardo una copia completa de los 502 archivos
del clon, incluidos `.git`, fuentes y compilaciones, en:

`C:/Users/gerad/Documents/ChatGPT/ROS2/backup_codigoroboraceragualat_2026-10-02`

Se compararon hashes SHA-256 de los 502 archivos del respaldo con el clon.
La integracion se aplica solo mediante archivos nuevos. Antes y despues se
verifica que los archivos preexistentes conservan los mismos bytes, salvo
los metadatos de Git necesarios para crear y activar la nueva rama.

## Comprobaciones y limites

Las pruebas C++ independientes cubren seleccion unica, ausencia de camara,
varias camaras, discriminacion D435/D435i/D435if, serie explicita, serie con
prefijo ROS, puerto USB, selectores contradictorios, etiquetas invalidas y
estadisticas temporales. Se comprueba la sintaxis de XML y de los launches.

La compilacion completa contra Jazzy/librealsense2 y las pruebas de USB,
RGB/profundidad/IMU se ejecutan en la Jetson con `tools/test_d435i.sh` y
`verify_camera`. Ninguna prueba local demuestra rendimiento a 70 km/h.
