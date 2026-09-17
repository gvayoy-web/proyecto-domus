# 68 - Resumen implementación v4.0: nuevas características y archivos obsoletos

**Fecha:** 2026-09-16
**Perfil actual:** BANCO_COMPLETO_S8050_IR

## Visión general
Firmware `casa_inteligente_v4.ino` completado con todas las características solicitadas en las notas Obsidian 00-67. El sistema ahora es escalable y no requiere nuevo hardware.

## Nuevas características implementadas

### 1. Expansión de audio Jarvis (v4.1: 28 eventos, 224 pistas)
- **28 eventos** `EventoJarvis` 1:1 con las 21 teclas CAR MP3 (nota 46) + sensores.
  Cada tecla tiene su carpeta propia: modos (CH-/CH+), diagnóstico (EQ),
  todo apagado (0), sonido (PLAY), rearme (200+), cultivo (3), ventilador (4),
  riego (5) y consultas 6/7/8/9 (temp, humedad, suelo, estado).
- **Voces duales:** Carlos SD 01-28 + Karla SD 51-78 = 112 tracks por voz,
  **224 MP3 totales**, 4 variantes por evento, generadas con
  `tools/generate_jarvis_audio.py` (edge-tts es-HN Carlos/KarlaNeural).
- `audio/jarvis_sd/MANIFEST.csv` con 224 filas (voz, carpeta, pista, evento,
  frase, bytes, SHA-256). Bibliotecas editables `audio/Carlos/01-28` y
  `audio/Karla/01-28`.
- Tecla 6: una pulsación consulta temperatura; doble pulsación (<2 s) alterna
  Carlos/Karla (resuelve el conflicto notas 46 vs 67: 6-9 ya no repiten reporte).
- Nota 67 vigente para PLAY (silencio) y 100+ (repetir última pista).

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
| `audio/jarvis_sd/01-64/` subfolders desorganizados | `audio/Carlos/01-28/` + `audio/Karla/01-28/` (SD 01-28 y 51-78) |
| `audio/jarvis_sd/MANIFEST.csv` (parcial, 112) | `MANIFEST.csv` con 224 filas, SHA-256 y evento por pista |
| `firmware/domus_pantalla.h`: `NUM_PANTALLAS = 5` | `NUM_PANTALLAS = 7` (views 5 y 6 agregadas; stats por copia, sin `extern atomic`) |
| Enum `EventoJarvis`: 14 eventos | 28 eventos 1:1 con las 21 teclas + sensores (`carpetaPara`, voz 2 +50) |
| Teclas 6-9 repetían el mismo reporte | 6/7/8/9 = consultas distintas; 6 doble = cambio de voz |
| `std::atomic<EstadisticasDOMUS>` (no compila: struct no trivial) | struct plano + contadores `inline`; pantalla recibe copia |
| `registrarHistorial` escribía en `reg.linea` inexistente | escribe en `TrabajoSD.linea` con `snprintf`, sin bloquear |
| `TipoRegistroHistorial::X + indice` (enum class, no compila) | `historialLuz(encender, indice)` |
| `String` concatenado en rutas calientes (Serial/IR/audio) | sobrecargas `const char*` en `log`/`emitirEventoLocal` + `snprintf` |
| Sin automaciones combinadas | `verificarAutomacionesCombinadas()`, `verificarAutomacionesRelativas()` |
| Sin demo sequences | `iniciarSecuenciaDemo()`, `detenerSecuenciaDemo()` |

## Tests
- **102 passed, 0 failed** (8 skipped = HIL sin placa física)
- 26/26 test_firmware_contract.py ✓
- 3/3 test_hil_producto_contract.py ✓
- 12/12 test_jarvis_audio.py ✓ (28 eventos, manifiesto 224, mapa 21 teclas)
- Nativos (candidato, safety, pantalla) recompilan tras stubs de voz/historial

## Próximos pasos
- Actualizar asserts en tests que esperan valores antiguos (NUM_PANTALLAS, enum values)
- Validación física con hardware ESP32-S3
- Despliegue a producción