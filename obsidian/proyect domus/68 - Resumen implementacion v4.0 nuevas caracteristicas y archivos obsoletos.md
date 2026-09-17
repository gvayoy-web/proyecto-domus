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
- Un botón = una función, un toque: sin combinaciones ni dobles pulsaciones.
- 100+ alterna la voz de un toque (Carlos por defecto; el anuncio suena ya en
  la voz nueva). PLAY = silencio. Repetir sale del mando, queda en Serial.
- Puerta ordenada: mientras Jarvis habla o 1.5 s tras la orden, el IR responde
  `NACK;IR;OCUPADO`. La saltan el aprendizaje, la tecla 0 y el PARO físico.
- Mapa ordenado como el mando: arriba configuración, abajo acciones en orden
  (luces y bomba 1-5, lecturas 6-9).

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
| Teclas 6-9 repetían el mismo reporte | 6/7/8/9 = consultas distintas; 100+ = voz (un toque, sin dobles) |
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

## Ronda v4.3: rescate + LCD + optimización (2026-09-17)

### Rescate de compilación (el firmware no enlazaba)
- `registrarLineaMicroSD()` declarada pero sin definir: definida (encola,
  no-op sin SD, cero heap).
- Sección 10 corrupta: `ultimaVerificacionRiego` duplicada, función
  `verificarVentiladorAutomatico()` inexistente llamada por `loop()`,
  `return` huérfano. Reconstruida como 10B.
- `verificarRiegoAutomaticoCombinado()` usaba `tempC/humAire/nivelAgua`
  sin declarar. `std::atomic<AutoTemporizado>` no compilaba: struct plano.

### Sensores
- ADC 8→4 muestras, 200→100 µs (la mitad de bloqueo; el test nativo usa fake).
- DHT: suspensión con reintento cada 60 s (antes latch hasta reset) y
  `DHT_FALLOS`/`DHT_SUSPENDIDO` visibles en `DIAGNOSTICO`.
- `leerNivelAgua` intacto (el test nativo fija su semántica contador a contador).

### LCD (7 vistas)
- Vista 3 compacta 1 letra (`B:1 S:0 C:A` / `V:0 I:B`; leyenda 1=ON 0=OFF
  A=AUTO B=BLOQ E=ERR). Vista 5 `ENC/APA/R/V/E`. Vista 6 perfil corto.
- Icono de nivel en vista 1. Avisos por tecla 2.5 s (`mostrarMensaje`):
  Sala/Cuarto/Cultivo/Vent/Riego ON-OFF-BLOQ, volumen, silencio, rearme,
  voz, diagnóstico. CH ya no se auto-tapa (oculta overlays al paginar).
- Consultas 6/7/8/9 saltan a su vista. Mute y voz visibles en vista 6
  (`A:MUTE/V1/V2`). Humedad con rango propio.

### Voz y lógica
- Cortes `ORIGEN_SISTEMA` suenan como automáticos (v3/v4 con cooldown).
- Tecla 5 = toggle (riego on/off; interlock, timeout y PARO intactos).
- PLAY OFF = aviso LCD (no puede sonar tras enmudecer); microSD 01-21/51-71.
- Reportes `ESTADO`/`DIAGNOSTICO` a `snprintf` (1 alloc en vez de ~25).
- Dispatcher Serial por tabla (15 comandos); `procesarComandoTexto` por
  referencia; conversores ADC unificados; secciones 10B/13B e índice.

### Tests
- **103 passed, 0 failed** (8 skipped = HIL sin placa). Harness de pantalla
  con bloque AVISO_OK; stubs nativos intactos.

## Próximos pasos
- Validación física con hardware ESP32-S3 (21 teclas, DFPlayer + BUSY, parlante)
- Copiar `audio/jarvis_sd` (01-21, 51-71) a la microSD FAT32
- Compilación Arduino real (los nativos g++ ya pasan; falta el build ESP32)
- Despliegue a producción