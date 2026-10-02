# vesc_teleop

Control del carro con un mando Bluetooth (o USB): Xbox, PS4, PS5, 8BitDo, etc.

```
mando --BT--> game_controller_node --/joy--> vesc_teleop_node --/vesc/motor_erpm--> vesc_control_node --> VESC
                                                              --/vesc/servo_cmd-->
```

## Controles (mapeo por defecto)

| Control | Acción |
| --- | --- |
| LB / L1 (mantener) | Hombre muerto: sin pulsarlo el motor queda libre (ERPM 0) y la dirección al centro |
| Stick izquierdo vertical | Acelerador (arriba adelante, abajo atrás), hasta `max_erpm` (5000) |
| RB / R1 (mantener) | Turbo: hasta `turbo_max_erpm` (10000) |
| Stick derecho horizontal | Dirección (servo 0.1 - 0.9) |

Si el mando se desconecta o deja de publicar durante `joy_timeout_sec` (0.5 s), el motor
pasa a ERPM 0. Al cerrar el teleop con Ctrl+C también se manda ERPM 0. Además,
`vesc_control_node` libera el motor si pasa `command_timeout_sec` (0.5 s) sin recibir
ERPM, por si el teleop se cae de golpe; por eso el teleop repite el ERPM a 20 Hz.

Todo se ajusta en `config/teleop.yaml`. Si el carro gira al revés, `invert_steering: true`.

## Emparejar el mando (una sola vez)

Pon el mando en modo emparejamiento (PS4/PS5: Share/Create + PS hasta que parpadee;
Xbox: botón de emparejar arriba). Después:

```bash
bluetoothctl
scan on            # espera a que aparezca el mando y copia su MAC
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
scan off
exit
```

Con `trust` se reconecta solo al encenderlo. Si `game_controller_node` no lo abre por permisos:

```bash
sudo usermod -aG input $USER   # y cerrar sesión / reiniciar
```

## Uso

Con las ruedas en el aire la primera vez:

```bash
ros2 launch vesc_teleop teleop.launch.py
```

Argumentos: `start_vesc:=false` si `vesc_control_node` ya está corriendo,
`vesc_port:=/dev/serial/by-id/...` para fijar el puerto (por defecto `auto`).
Para comprobar los índices de tu mando: `ros2 topic echo /joy`.
