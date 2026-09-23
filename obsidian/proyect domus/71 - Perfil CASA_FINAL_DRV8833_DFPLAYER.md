# 71 - Perfil CASA_FINAL_DRV8833_DFPLAYER (Perfil 4)

**Fecha:** 2026-09-18
**Placa:** ESP32-S3 N16R8 en COM9 (CH343)
**Perfil anterior:** `BANCO_COMPLETO_S8050_IR` (perfil 3)
**Perfil actual:** `CASA_FINAL_DRV8833_DFPLAYER` (perfil 4)

## Resumen de cambios

Se implementó el perfil `CASA_FINAL_DRV8833_DFPLAYER` con los módulos
que llegaron según la nota Obsidian 39 y el inventario fotografiado.

### Asignación de pines (perfil 4)

| GPIO | Función anterior | Función perfil 4 |
|---|---|---|
| 3 | LDR | LDR (conserva) |
| 4 | Bomba (S8050) | DRV8833 AIN1 (bomba) |
| 5 | Luz sala | DRV8833 BIN1 (ventilador dir) |
| 6 | Luz cuarto | DRV8833 BIN2 (ventilador dir) |
| 7 | Ventilador (bloqueado) | DRV8833 AIN2 (bomba dir) |
| 8 | Luz cultivo | 74HC595 DS (datos shift register) |
| 9 | PIR | PIR (conserva) |
| 10 | PARO | PARO (conserva) |
| 11 | SILENCIO | 74HC595 SHCP (clock shift) |
| 12 | IR | IR (conserva) |
| 13 | LCD SCL | LCD SCL (conserva) |
| 14 | DHT11 | DHT11 (conserva) |
| 15 | Suelo | Suelo (conserva) |
| 16 | Nivel | Nivel (conserva) |
| 17 | LCD SDA | LCD SDA (conserva) |
| 18 | MODO | 74HC595 STCP (latch shift) |

### Módulos añadidos

1. **DRV8833** (puente H dual):
   - Canal A: bomba → AIN1=GPIO4, AIN2=GPIO7
   - Canal B: ventilador → BIN1=GPIO5, BIN2=GPIO6
   - nSLEEP → VCC (no usa GPIO)
   - Dirección HIGH/LOW para forward/reverse/brake

2. **74HC595 shift register** (para luces):
   - DS=GPIO8, SHCP=GPIO11, STCP=GPIO18
   - Q0→Luz sala, Q1→Luz cuarto, Q2→Luz cultivo
   - Los botones MODO y SILENCIO se sacrifican como pines del shift register
   - `actualizarLuces74HC595()` maneja el envío de datos

3. **DFPlayer Mini** (audio):
   - UART1 remapeada a GPIO4(RX)/GPIO7(TX)
   - Comparte pines con DRV8833
   - `MP3_HABILITADO` se habilita automáticamente en perfil 4

### Cambios en el firmware

#### `domus_drivers.h`
- `DOMUS_DRIVER=1` y `DOMUS_DRIVER_VALIDADO=1` activos para perfil 4
- `driverMotoresAplicar()` ahora controla GPIOs del DRV8833
- `DRIVER_MOTORES_LISTO` verifica perfil 4 + DRV8833 validado
- Pines DRV8833 definidos como constantes

#### `domus_jarvis_audio.h`
- `AUDIO_CANDIDATO_HABILITADO` verifica `DOMUS_PERFIL_CASA == 4`

#### `casa_inteligente_v4.ino`
- Nuevo `PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER` (valor 4)
- `SALIDA_FISICA_CASA[3]` = `true` para perfil 4 (ventilador habilitado)
- `BOMBA_DIRECTA_S8050` = `false` para perfil 4
- `IR_CASA_HABILITADO` = `true` para perfiles 3 y 4
- `PINES_RESERVADOS_DOMUS` sin cambios (74HC595 usa GPIOs ya reservados)
- `revisarControlesFisicos()` bifurcada para perfil 4 (lee botones via shift register)
- `actualizarLuces74HC595()` implementada con pinMode switching
- Setup inicializa DRV8833 y 74HC595 para perfil 4
- `ejecutarOrdenActuador()` llama `driverMotoresAplicar()` para motores

#### `domus_esqueleto/domus_esqueleto.ino`
- Nuevo esqueleto para perfil 4

### Comportamiento del perfil 4

- **Bomba y ventilador** usan DRV8833 en vez de S8050
- **Luces** usan 74HC595 en vez de GPIO directo
- **Audio** habilitado con DFPlayer Mini
- **IR** sigue habilitado (GPIO12)
- **LCD** sin cambios (GPIO13/17)
- **Sensores** sin cambios (suelo, nivel, DHT11, LDR, PIR)
- **Botones MODO y SILENCIO** funcionan via 74HC595 con pinMode switching

### Consideraciones eléctricas

- DRV8833 necesita alimentación separada de 5V para los motores
- nSLEEP del DRV8833 conectado a VCC
- 74HC595 alimentado a 3.3V
- DFPlayer alimentado a 3.3V o 5V según su especificación
- GND común entre ESP32, DRV8833, 74HC595 y DFPlayer
- Capacitores de desacoplo cerca de DRV8833 y DFPlayer
- Fusible en serie con la alimentación de motores

### Puertas F1-F7 restantes

- F1: Validación física del DRV8833 (foto + medición de tabla de verdad)
- F5: Validación física del DFPlayer (probado con 01/001.mp3)
- F6: Integración de JarvisAudio con luces, luego riego y alertas
- HIL: Prueba con LCD, sensores, luces, motores y audio
- Diagrama final actualizado con GPIO confirmados

### Relacionadas

- [[66 - Arquitectura final software hardware y funciones]]
- [[69 - Placa real lado usable y restriccion de GPIO]]
- [[70 - Mejoras firmware 2026-09-18]]
- [[39 - Inventario fotografiado y pines visibles]]

---

## Visualizadores 3D y Diagramas

- `../firmware/CONEXIONES_FIRMWARE_PERFIL4.md` — Documento completo de conexiones
- `../hardware/planos/modelo_3d_con_componentes.html` — Gemelo digital 3D con componentes
- `../hardware/planos/diagrama_conexiones.html` — Diagrama eléctrico de conexiones
