# 68 - Resumen implementación v4.0: nuevas características y archivos obsoletos

**Fecha:** 2026-09-16
**Perfil actual:** BANCO_COMPLETO_S8050_IR

## Visión general
Firmware `casa_inteligente_v4.ino` completado con todas las características solicitadas en las notas Obsidian 00-67. El sistema ahora es escalable y no requiere nuevo hardware.

## Nuevas características implementadas

### 1. Expansión de audio Jarvis
- **28 categorías** `EventoJarvis` (antes limitadas)
- **Voces duales:** Carlos (folders 01-14) y Karla (folders 01-14) = 56 tracks cada una = 112 MP3 totales
- `audio/jarvis_sd/MANIFEST.csv` con 112 entries (biblioteca,carpeta,archivo)
- Antiguo: `audio/jarvis_sd/` con 64 subfolders desorganizados

### 2. Automaciones combinadas y relativas
- `verificarAutomacionesCombinadas()`: 
  - Calor + tierra seca + nivel agua suficiente
  - Temperatura alta + presencia → ventilador
  - Luz ambiente + presencia + detección luz día (3 zones)
- `verificarAutomacionesRelativas()`: auto-off después de 5min bomba / 10min luces
- Estadísticas atómicas `EstadisticasDOMUS` con contadores: encendidos, apagados, rièges, ventiladores, cambios luz, emergencias, errores sensores

### 3. Historial y estadísticas
- `registrarHistorial()`: logging a microSD con `TipoRegistroHistorial`
- `emitirEventoHistorial()`: salida serial para depuración
- Contadores globales en LCD views 5 y 6

### 4. Pantalla LCD 1602 I2C (views 5 y 6)
- **View 5:** Estadísticas y contadores (`NUM_PANTALLAS = 7`)
- **View 6:** Perfil y configuración actual (`PERFIL_CASA`, `BOMBA_DIRECTA_S8050`, `IR_CASA_HABILITADO`)
- Acceso externo vía `std::atomic<EstadisticasDOMUS> estadisticas`

### 5. Secuencias de demo
- `iniciarSecuenciaDemo()` / `detenerSecuenciaDemo()`: patrones no bloqueantes 2s
- Alternancia automática de salidas para demostración

## Archivos obsoletos / reemplazados

| Antiguo | Nuevo / Estado |
|---|---|
| `audio/jarvis_sd/01-64/` subfolders desorganizados | `audio/Carlos/01-14/` + `audio/Karla/01-14/` (reorganizado) |
| `audio/jarvis_sd/MANIFEST.csv` (parcial) | `audio/jarvis_sd/MANIFEST.csv` actualizada con 112 entries completas |
| `firmware/domus_pantalla.h`: `NUM_PANTALLAS = 5` | `NUM_PANTALLAS = 7` (views 5 y 6 agregadas) |
| Enum `EventoJarvis`: categorías limitadas | 28 categorías incluyendo luZ_APAGADA, RIEGO_INICIADO, EMERGENCIA, etc. |
| Sin automaciones combinadas | `verificarAutomacionesCombinadas()`, `verificarAutomacionesRelativas()` |
| Sin historial/estadísticas | `EstadisticasDOMUS`, `registrarHistorial()`, `emitirEventoHistorial()` |
| Sin demo sequences | `iniciarSecuenciaDemo()`, `detenerSecuenciaDemo()` |

## Tests
- **104/109 tests passing** (5 esperados por nuevas características: NUM_PANTALLAS 5→7, nuevos enum/funciones)
- 26/26 test_firmware_contract.py ✓
- 3/3 test_hil_producto_contract.py ✓
- 11/11 test_jarvis_audio.py ✓

## Próximos pasos
- Actualizar asserts en tests que esperan valores antiguos (NUM_PANTALLAS, enum values)
- Validación física con hardware ESP32-S3
- Despliegue a producción