# RoboRacer: VESC, Hokuyo y RealSense D435i

Destino: Ubuntu 24.04 de NVIDIA / Jetson ARM64, ROS 2 Jazzy, C++17.
El codigo probado sigue en `scr/vesc_control` y `scr/scr_bringup`.
La integracion de camara se anade como paquete independiente `scr/d435i_bringup`.

El LiDAR se utiliza para SLAM y el mapa; la futura IA debera consumir los datos
y decidir cambios de rumbo. Actualmente no hay IA entrenada, odometria ni
control autonomo terminado. El launch original publica `odom -> base_link`
como transformacion fija provisional, no como odometria medida. La camara y
su IMU anaden datos, pero no calculan odometria ni control por si solas.

La deteccion USB y la comprobacion de flujos son ejecutables C++. El driver
oficial `realsense2_camera` tambien es C++. Los archivos `.launch.py` solo
orquestan el arranque, como ya hace el launch original del Hokuyo.

## Instalar dependencias en la Jetson

Con ROS 2 Jazzy instalado y sus repositorios configurados:

```bash
sudo apt update
sudo apt install ros-jazzy-realsense2-camera ros-jazzy-librealsense2 \
  ros-jazzy-tf2-ros ros-jazzy-lifecycle-msgs ros-jazzy-rviz2 \
  python3-colcon-common-extensions
```

Los drivers `urg_node` y `slam_toolbox` deben seguir instalados para el arranque
combinado. Si la imagen NVIDIA requiere ajustar el SDK o los permisos USB,
usar la [guia oficial Jetson](https://github.com/realsenseai/librealsense/blob/master/doc/installation_jetson.md).
No mezclar versiones del SDK instaladas manualmente con las del paquete ROS.

## Compilar y probar sin tocar las compilaciones actuales

Desde la raiz `ROS2` del clon, ya en la rama `codex/d435i-jazzy`:

```bash
bash tools/test_d435i.sh
source install_d435i/setup.bash
ros2 run d435i_bringup inspect_devices
ros2 launch d435i_bringup d435i.launch.py
```

El script construye un overlay en `build_d435i`, `install_d435i` y `log_d435i`.
No elimina ni modifica los directorios `build`, `install` o `log` existentes.
Tampoco recompila ni arranca la VESC.
`inspect_devices` y `verify_camera` no publican comandos de motor o direccion.

En otra terminal, con los mismos entornos sourceados:

```bash
source /opt/ros/jazzy/setup.bash
source install_d435i/setup.bash
ros2 run d435i_bringup verify_camera
```

Este comprobador observa durante 5 segundos RGB, profundidad e IMU. Muestra
frecuencias, intervalos maximos y antiguedad de los mensajes segun timestamps
ROS; devuelve codigo 2 si faltan flujos o han dejado de llegar (silencio
maximo configurable, 1 segundo por defecto). La antiguedad requiere relojes
comparables y no representa por si sola la latencia total hasta actuar sobre
el motor. La comprobacion de flujos no impone limites de rendimiento.

## Identificar la camara y los puertos

La D435i usa USB para imagen, profundidad e IMU. No se enlaza a un puerto
COM/TTY. `inspect_devices` enumera `/dev/ttyACM*`, `/dev/ttyUSB*` y sus rutas
estables para distinguirlos de la camara, sin abrirlos ni mandar comandos.
La camara se selecciona por su serie leida mediante el SDK RealSense.

```bash
# Seleccion automatica si existe exactamente una D435i:
ros2 launch d435i_bringup d435i.launch.py

# Con varias camaras, reemplazar SERIE_REAL por la serie que muestra el inventario:
ros2 launch d435i_bringup d435i.launch.py serial_no:=SERIE_REAL

# Opcional: fijar tambien el puerto USB de la Jetson:
ros2 launch d435i_bringup d435i.launch.py usb_port_id:=2-1.3
```

`K38179-111` es la etiqueta de modelo suministrada, no la serie USB que espera
el driver. Con ninguna camara o varias sin selector, el launch explica el
problema y no vincula una camara incorrecta. La comprobacion USB ocurre al
arrancar; la reconexion posterior la gestiona el driver con la serie elegida.

## Perfiles y rendimiento para RoboRacer

`scr/d435i_bringup/config/d435i.yaml` solicita profundidad 848x480 a 90 FPS,
color 640x480 a 60 FPS e IMU. Estos perfiles requieren USB 3 y deben comprobarse
con el firmware y SDK instalados. El driver puede usar un perfil alternativo
si el solicitado no esta disponible; revisar sus logs y las frecuencias
medidas por `verify_camera`.

El alineado profundidad-color y la nube de puntos estan desactivados para
evitar procesamiento adicional. Los flujos usan QoS `SENSOR_DATA`; el
comprobador usa una cola de una muestra. Si se requiere alineado para un
algoritmo posterior, activarlo explicitamente en el YAML y volver a medir.

A 70 km/h, el vehiculo recorre aproximadamente 0.22 m entre frames de
profundidad a 90 Hz, 0.32 m entre frames RGB a 60 Hz y 1.94 cm por milisegundo.
Estas cifras son de tiempo de muestreo, no garantias de seguridad o de latencia
del sistema. Esta integracion no implementa percepcion, planificacion ni el
control autonomo del carro.

Topicos predeterminados:

```text
/camera/d435i/color/image_raw
/camera/d435i/color/camera_info
/camera/d435i/depth/image_rect_raw
/camera/d435i/depth/camera_info
/camera/d435i/imu
```

## Montaje y arranque combinado

Medir posicion y orientacion de la camara respecto a `base_link`. Los valores
predeterminados de cero son marcadores hasta medir el montaje real.

```bash
ros2 launch d435i_bringup d435i.launch.py \
  camera_x:=0.0 camera_y:=0.0 camera_z:=0.0 \
  camera_roll:=0.0 camera_pitch:=0.0 camera_yaw:=0.0

# LiDAR + SLAM original + camara; no arranca la VESC:
ros2 launch d435i_bringup vehicle_sensors.launch.py

# Solo camara con el launch combinado:
ros2 launch d435i_bringup vehicle_sensors.launch.py enable_lidar:=false
```

El driver publica sus TF internos y el nuevo launch anade `base_link ->
d435i_link`. Si ya existe esa transformacion en otro nodo, usar
`publish_mount_tf:=false` en el launch de camara independiente.

## Preparar una subida de la rama

La integracion queda en un commit local de `codex/d435i-jazzy`.
Solo los archivos nuevos forman parte de ese commit. El clon ya tenia
372 cambios locales antes de este trabajo, principalmente archivos generados
y diferencias de finales de linea. No incluirlos con `git add .`.

```bash
# Revisar el commit de integracion y subir la rama cuando este aprobada:
git show --stat HEAD
git push -u origin codex/d435i-jazzy
```

Si haces ajustes despues de las pruebas en la Jetson, incluir solo sus
archivos, sin incorporar las compilaciones originales:

```bash
git add .gitignore README.md docs/comparacion_codigos.md \
  scr/d435i_bringup tools/test_d435i.sh
git diff --cached --stat
git commit -m "Adjust D435i configuration after Jetson tests"
git push -u origin codex/d435i-jazzy
```

La rama se prepara localmente; la subida y la prueba fisica se hacen despues
de revisar los cambios y encender la Jetson.

Referencias: [driver oficial ROS 2](https://github.com/realsenseai/realsense-ros),
[parametros del driver](https://github.com/realsenseai/realsense-ros/blob/ros2-master/README.md#parameters).
