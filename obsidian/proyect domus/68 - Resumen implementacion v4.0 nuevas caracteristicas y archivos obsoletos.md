# 68 - Resumen implementación v4.0: nuevas características y archivos obsoletos

**Fecha:** 2026-09-16
**Perfil actual:** BANCO_COMPLETO_S8050_IR

## Visión general
Firmware `casa_inteligente_v4.ino` completado con todas las características solicitadas en las notas Obsidian 00-67. El sistema ahora es escalable y no requiere nuevo hardware.

## Nuevas características implementadas

### 1. Audio 1:1 por botón (v4.2: 21 eventos, 168 pistas)
- **21 eventos** `EventoJarvis`, uno por botón (códigos físicos nota 64,
  acciones nota 46). Carpeta = botón: CH- = 01 modo manual, CH = 02 página,
  CH+ = 03 modo auto, Anterior/1 = sala, Play = 05 silencio, Siguiente/2 =
  cuarto, VOL = 07/08, EQ = 09 diagnóstico, 0 = 10 apagado/PARO, 100+ = 11
  repetir, 200+ = 12 rearme, 3 = 15 cultivo, 4 = 16 ventilador, 5 = 17 riego,
  6/7/8/9 = 18/19/20/21 consultas y estado.
- **Voces duales:** Carlos SD 01-21 + Karla SD 51-71 = 84 tracks por voz,
  **168 MP3 totales**, 4 variantes por botón, generadas con
  `tools/generate_jarvis_audio.py` (edge-tts es-HN Carlos/KarlaNeural).
- `audio/jarvis_sd/MANIFEST.csv` con 168 filas (voz, carpeta, pista, evento,
  frase, bytes, SHA-256). Bibliotecas editables `audio/Carlos/01-21` y
  `audio/Karla/01-21`.
- Variantes con significado: conmutadores 1 = ON manual, 2 = OFF manual,
  3 = ON automático, 4 = OFF automático; botón 0: 1-2 apagado, 3-4 PARO;
  EQ 3-4 fallo de sensor; tecla 8 (v4) depósito bajo; tecla 9 (v3)
  "Sistemas en línea" (arranque y cambio de voz).
- CH- fija modo manual y CH+ modo automático en todas las salidas (nota 46).
- Tecla 6: una pulsación consulta temperatura; doble (<2 s) alterna voces.
- Nota 67 vigente para PLAY (silencio) y 100+ (repetir o "nada que repetir").

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
| `audio/jarvis_sd/01-64/` subfolders desorganizados | `audio/Carlos/01-21/` + `audio/Karla/01-21/` (SD 01-21 y 51-71) |
| `audio/jarvis_sd/MANIFEST.csv` (parcial, 112) | `MANIFEST.csv` con 168 filas, SHA-256 y evento por pista |
| `firmware/domus_pantalla.h`: `NUM_PANTALLAS = 5` | `NUM_PANTALLAS = 7` (views 5 y 6 agregadas; stats por copia, sin `extern atomic`) |
| Enum `EventoJarvis`: 14 eventos | 21 eventos, uno por botón (`carpetaPara`, voz 2 +50, variantes y grupos) |
| Teclas 6-9 repetían el mismo reporte | 6/7/8/9 = consultas distintas; 6 doble = cambio de voz |
| `std::atomic<EstadisticasDOMUS>` (no compila: struct no trivial) | struct plano + contadores `inline`; pantalla recibe copia |
| `registrarHistorial` escribía en `reg.linea` inexistente | escribe en `TrabajoSD.linea` con `snprintf`, sin bloquear |
| `TipoRegistroHistorial::X + indice` (enum class, no compila) | historial directo sin aritmética de enums |
| `String` concatenado en rutas calientes (Serial/IR/audio) | sobrecargas `const char*` en `log`/`emitirEventoLocal` + `snprintf` |
| Sin automaciones combinadas | `verificarAutomacionesCombinadas()`, `verificarAutomacionesRelativas()` |
| Sin demo sequences | `iniciarSecuenciaDemo()`, `detenerSecuenciaDemo()` |

## Tests
- **99-102 passed, 0 failed** (skips = HIL sin placa + flake AV de Windows)
- 26/26 test_firmware_contract.py ✓
- 3/3 test_hil_producto_contract.py ✓
- 12/12 test_jarvis_audio.py ✓ (21 eventos, manifiesto 168, mapa 21 botones)
- Nativos (candidato, safety, pantalla) recompilan tras stubs de voz/historial

## Próximos pasos
- Validación física con hardware ESP32-S3 (21 teclas, DFPlayer, parlante)
- Copiar `audio/jarvis_sd` (01-21, 51-71) a la microSD FAT32
- Despliegue a producción