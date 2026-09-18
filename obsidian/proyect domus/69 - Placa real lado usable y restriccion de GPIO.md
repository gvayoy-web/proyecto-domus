# 69 - Placa real: solo el lado izquierdo es usable

**Fecha:** 2026-09-18
**Placa:** ESP32-S3-WROOM-1 N16R8 (devkit con CH343 para USB-serial, COM9)
**Autoridad:** fotos de la placa + esquemático aportados por Isaac

## Restricción física (manda sobre cualquier plan)

Solo se puede usar **un lado** del devkit: el que trae **doble 3V3 arriba y
5V0 + GND abajo**. El otro lado está descartado.

| Lado | Pines | Estado |
|---|---|---|
| Izquierdo (3V3, 3V3, RST … 5V0, GND) | GPIO 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 | **USABLE** |
| Derecho (GND … GPIO43/44/1/2/42–35/0/45/48/47/20/19/GND) | GPIO 0, 1, 2, 19, 20, 35–48 | **PROHIBIDO** |

Notas:
- GPIO19/20 son USB D-/D+ nativo: irrelevantes (esta placa habla por CH343
  externo, no por USB nativo). Igual quedan prohibidos por estar del lado malo.
- El LED RGB (GPIO48) queda descartado por la misma razón.
- El esquemático confirma auto-reset por DTR/RTS (CH343 → EN/GPIO0): no tocar.

## Mapa vigente verificado contra la restricción (2026-09-18)

| GPIO | Función | Lado |
|---|---|---|
| 3 | LDR | izq ✓ |
| 4 | Bomba (base S8050) | izq ✓ |
| 5 / 6 / 8 | Sala / cuarto / cultivo | izq ✓ |
| 7 | Ventilador (bloqueado) | izq ✓ |
| 9 | PIR | izq ✓ |
| 10 / 11 / 18 | PARO / silencio / modo | izq ✓ |
| 12 | IR HX1838 | izq ✓ |
| 13 / 17 | LCD SCL / SDA (0x27) | izq ✓ |
| 14 | DHT11 DATA | izq ✓ |
| 15 / 16 | Suelo / nivel | izq ✓ |

Resultado: **16/16 dentro del lado usable, 0 fuera.** No hubo que mover nada.

## Hallazgo: cero GPIO libres en el lado usable

Los 16 GPIO del lado izquierdo están todos asignados. Para el perfil futuro
(`CASA_FINAL_DRV8833_DFPLAYER`) faltan: 4 entradas DRV8833 (AIN1/AIN2/BIN1/BIN2)
+ 2–3 DFPlayer (RX/TX/BUSY). Opciones cuando llegue el hardware (sin decidir):
- a) Reutilizar GPIO4/7 como entradas del DRV (bomba/vent dejan el S8050) y
  sacrificar 2 botones (p. ej. MODO→CH asume paginado, SILENCIO→PLAY asume).
- b) Expansor (74HC595 ya en inventario) para liberar pines digitales.
- c) DFPlayer por software-serial en pines compartidos (riesgoso, último recurso).

## GPIO16 (misterio del pin cargado)

PINTEST 7 (vacío real) marca FLOTANTE perfecto; GPIO16 con todo desconectado
marca 495/2727/425. Con la restricción de esta nota, el sospechoso pasa a ser
la fila compartida de la protoboard o el propio devkit, no el chip en general.
Test guardián: `test_lado_usable` (solo GPIO 3–18 en `MAPA_CASA`).

Relacionadas: [[55 - Correccion auditoria 5 mapa fuente driver]],
[[64 - Sesion fisica COM9 LCD IR sensores y bomba]].
