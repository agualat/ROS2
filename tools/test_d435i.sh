#!/usr/bin/env bash
# Build into new directories; do not remove or overwrite the existing builds.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "$(uname -s)" != Linux ]]; then
  echo "Ejecutar en Ubuntu 24.04 de la Jetson." >&2
  exit 2
fi
source /etc/os-release
if [[ "${VERSION_ID:-}" != 24.04 ]]; then
  echo "Este procedimiento apunta a Ubuntu 24.04; version actual: ${VERSION_ID:-?}" >&2
  exit 2
fi
if [[ ! -f /opt/ros/jazzy/setup.bash ]]; then
  echo "No se encontro ROS 2 Jazzy en /opt/ros/jazzy." >&2
  exit 2
fi
set +u
source /opt/ros/jazzy/setup.bash
set -u
ros2 pkg prefix realsense2_camera >/dev/null
command -v colcon >/dev/null
command -v ctest >/dev/null
colcon --log-base "$repo_root/log_d435i" build \
  --base-paths "$repo_root/scr/d435i_bringup" "$repo_root/scr/scr_bringup" \
  --build-base "$repo_root/build_d435i" \
  --install-base "$repo_root/install_d435i" \
  --symlink-install --event-handlers console_direct+
ctest --test-dir "$repo_root/build_d435i/d435i_bringup" --output-on-failure
echo "Compilacion y pruebas C++ terminadas."
echo "Ejecuta: source \"$repo_root/install_d435i/setup.bash\""
echo "Puertos: ros2 run d435i_bringup inspect_devices"
echo "Camara: ros2 launch d435i_bringup d435i.launch.py"
echo "En otra terminal: ros2 run d435i_bringup verify_camera"
