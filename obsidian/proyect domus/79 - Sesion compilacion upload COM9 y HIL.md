---
estado: vigente
fecha: 2026-09-22
autoridad: 79
tipo: sesion-compilacion-upload-hil
depends: 77, 78
---

# 79 - Sesion compilacion, upload COM9 y HIL

> [!IMPORTANT]
> Sesión donde se corrigieron errores de compilación, se subió el firmware al
> ESP32 en COM9 y se alinearon los tests HIL con los nombres actuales del firmware.

## Resumen de la sesión

### FASE 1: Corrección de tests Python (antes de compilar)

Se corrigieron 6 fallas en tests tras los cambios del firmware:

| Test | Error | Corrección |
|------|-------|------------|
| `test_casa_candidato` | Esperaba `dfRx/dfTx` | Restaurado a `micOff/demo` (nombres reales del struct) |
| `test_firmware_contract` | Esperaba `LUCES_AUTO_BLOQUEADAS_SENSOR` | Cambiado a `LUCES_BLOQUEADAS` |
| `test_pantalla_final` | Esperaba `B:0 S:0 C:0` | Cambiado a `B:0 Ca:0 P:0` (formato actual) |
| `native_integration.cpp` | `PINES_SALIDAS={4,5,6,7,8}` | Corregido a `{4,5,8,7,8}` (igual a `MAPA_CASA.salidas`) |
| `test_jarvis_audio` | Esperaba pines DFPlayer viejos | Actualizado a 11/18 |
| `test_hil_producto` | Esperaba `Luz Sala=` etc. | Actualizado a `Casa=`, `Porche=`, `Cultivo=`, `Spare=` |

**Resultado:** 106 passed, 8 skipped, 0 failed (commit `db28112`).

### FASE 2: Errores de compilación corregidos

Al intentar compilar con `arduino-cli`, aparecieron 3 errores:

#### Error 1: GPIO duplicado
```
static assertion failed: Hay GPIO duplicados en el mapa DOMUS
```
- **Causa:** `MAPA_CASA.spare=8` conflictaba con `MAPA_CASA.porche=8`
- **Solución:** Asignado `spare` a **GPIO6** (libre en costado izquierdo)
- **Actualizado:** `static_assert` y `salidas={4,5,8,7,6}`

#### Error 2: DRV8833_PIN_BIN1/BIN2 no declarados
```
error: 'DRV8833_PIN_BIN1' was not declared in this scope
```
- **Causa:** Ventilador eliminado pero código aún usaba esos pines
- **Solución:** Canal B del DRV8833 ahora `return false` (sin acción)
- **Líneas corregidas:** 592-593 (driverMotoresAplicarFinal) y 2441-2446 (setup)

#### Error 3: ResultadoDiagnostico incomplete type
```
error: 'ResultadoDiagnostico' does not name a type
```
- **Causa:** Arduino preprocessor genera forward declarations ANTES de la definición del struct
- **Solución:** Movido `struct ResultadoDiagnostico` antes de los `#include` (línea ~125)
- **Eliminada:** definición duplicada posterior

### FASE 3: Compilación y upload exitoso

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  --upload --port COM9 casa_inteligente_v4.ino
```

**Resultado:**
- Flash: 429,079 bytes (13% de 3,145,728)
- RAM: 29,404 bytes (8% de 327,680)
- Chip: ESP32-S3 QFN56 revision v0.2
- PSRAM: 8MB embedded (AP_3v3)
- MAC: dc:b4:d9:10:48:20
- Upload completado y verificado

### FASE 4: Tests HIL alineados

Los tests HIL (`test_hil_producto.py`) esperaban nombres viejos:

```python
# ANTES (viejo):
LED_COMMANDS = (("LUZ1", "Luz Sala"), ("LUZ2", "Luz Cuarto"), ("INVER", "Luz Inv."))
MOTOR_COMMANDS = (("RIEGO_ON", "Bomba"), ("VENT_ON", "Ventilador"))

# DESPUÉS (actual):
LED_COMMANDS = (("LUZ1", "Casa"), ("LUZ2", "Porche"), ("INVER", "Cultivo"))
MOTOR_COMMANDS = (("RIEGO_ON", "Bomba"), ("VENT_ON", "Spare"))
```

También se eliminó `PIR=` del test (sensor PIR no existe físicamente).

**Resultado:** 102 non-HIL passed, 8 HIL skipped (puerto no detectado post-reset).

### FASE 5: LED RGB invernadero (PREGUNTA ABIERTA)

El usuario preguntó sobre usar un **LED RGB multicolor de 3/4 patas** en
reemplazo de los 3 LEDs del invernadero (cultivo) para un efecto "wow".

**Datos conocidos:**
- LED de 3 patas (o 4), una es más grande = **GND (cátodo común)**
- Tipo: **Cátodo común** (pin común al GND, se enciende con HIGH)

**Resistencias calculadas (3.3V ESP32, cátodo común):**

| Color | Vf típico | Resistencia | Corriente |
|-------|-----------|-------------|-----------|
| Rojo | 1.8-2.2V | **220Ω** | ~15mA |
| Verde | 2.8-3.3V | **100Ω** | ~15mA |
| Azul | 3.0-3.3V | **100Ω** | ~15mA |

**Conexión:**
```
ESP32 GPIO_R ──[220Ω]──┤
ESP32 GPIO_G ──[100Ω]──┤ LED RGB
ESP32 GPIO_B ──[100Ω]──┤
GND ────────────────────┘ (pin común/cátodo)
```

**Problema:** GPIO7 (cultivo actual) es 1 solo pin. RGB necesita 3 pines PWM.
Pines candidatos del costado derecho: GPIO19, GPIO20, GPIO21.

**Estado:** Pregunta pendiente de decisión del usuario (no se respondió).

## Estado actual

- [x] Tests Python: 106 passed, 0 failed
- [x] Compilación: exitosa (13% flash, 8% RAM)
- [x] Upload a COM9: completado y verificado
- [x] Tests HIL: nombres alineados (skip por puerto)
- [ ] LED RGB invernadero: decisión pendiente
- [ ] Verificación post-upload con monitor serial

## Comandos útiles

```powershell
# Compilar
& "C:\Users\Isaac\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" `
  compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" `
  "firmware\casa_inteligente_v4\casa_inteligente_v4.ino"

# Subir
& "...\arduino-cli.exe" compile --fqbn "..." --upload --port COM9 "...\casa_inteligente_v4.ino"

# Tests
python -m pytest firmware/tests/ --tb=short -k "not hil_producto"
$env:DOMUS_PORT="COM9"; python -m pytest firmware/tests/test_hil_producto.py -v
```
