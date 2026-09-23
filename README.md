# PROJECT DOMUS
## Casa Inteligente Local para Maqueta ESP32-S3

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-ff69b4.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Arduino%20ESP32-3.3.10-004422.svg)](https://github.com/espressif/arduino-esp32)
[![Tests](https://img.shields.io/badge/Tests-102_passed_0_failed-brightgreen.svg)](firmware/tests)
[![Profile](https://img.shields.io/badge/Profile-CASA_FINAL_DRV8833_DFPLAYER-green.svg)](obsidian/proyect%20domus/71%20-%20Perfil%20CASA_FINAL_DRV8833_DFPLAYER.md)
[![Version](https://img.shields.io/badge/Version-v4.0-4A90E2.svg)](https://github.com/Isaac/casa_inteligente_v4/commits/proyecdomus)
[![Lines](https://img.shields.io/badge/Code-1.9K-orange.svg)](firmware/casa_inteligente_v4/casa_inteligente_v4.ino)
[![Obsidian](https://img.shields.io/badge/Docs-Obsidian-9944FF.svg)](obsidian/proyect%20domus)

---

Casa inteligente local para una maqueta con **ESP32-S3 N16R8** (16MB Flash / 8MB PSRAM OPI). El firmware `casa_inteligente_v4.ino` está completo con todas las características solicitadas en las notas Obsidian 00-67. El sistema es **escalable y no requiere nuevo hardware**.

## 📦 Estado Actual

| Métrica | Valor |
|---|---|
| **Tests pasando** | 49+ passed, 0 failed ✅ (contract, HIL, audio, pantalla) |
| **Tests contrato** | 27/27 ⭐ |
| **Tests audio** | 13/13 ⭐ |
| **Tests pantalla** | 5/5 ⭐ |
| **Líneas de firmware** | ~2,700 |
| **Firmware Flash** | 14% |
| **Firmware RAM** | 8% |
| **Perfil actual** | `CASA_FINAL_DRV8833_DFPLAYER` (perfil 4) |
| **Firmware en COM9** | Perfil 4 code listo, pendiente compilar/upload |

> **Nota 70**: Mejoras de firmware 2026-09-18: DIAGNOSTICO ampliado, `PINTEST_ALL`, optimización de automatizaciones combinadas, corrección de indentación y tests. Ver [[70 - Mejoras firmware 2026-09-18]].

> **Nota 71**: Perfil `CASA_FINAL_DRV8833_DFPLAYER` implementado con DRV8833, 74HC595 y DFPlayer. Ver [[71 - Perfil CASA_FINAL_DRV8833_DFPLAYER]].

## ✨ Características Implementadas

### Audio 1:1 por botón (21 eventos)
- **168 MP3 tracks** (21 botones x 4 variantes x 2 voces): **Carlos** SD 01-21 y **Karla** SD 51-71
- Cada botón del mando CAR MP3 tiene su carpeta (códigos nota 64, acciones nota 46)
- `MANIFEST.csv` con 168 filas (voz, carpeta, pista, evento, frase, SHA-256)
- Antes: 64 subfolders desorganizados

### Automatizaciones Inteligentes
- **Automaciones combinadas**: calor+tierra_seca+agua, temp+presencia, luz+presencia+zonas_día
- **Automaciones relativas**: auto-off después de 5min bomba / 10min luces
- `EstadisticasDOMUS` (struct plano, sin bloqueos) con contadores: encendidos, apagados, riegos, ventilador, cambios luz, emergencias, errores sensores

### Pantalla LCD 1602 I2C (7 views)
- **View 5**: Estadísticas y contadores (`NUM_PANTALLAS = 7`)
- **View 6**: Perfil y configuración (`PERFIL_CASA`, `BOMBA_DIRECTA_S8050`, `IR_CASA_HABILITADO`)
- La pantalla recibe los contadores por copia en `DatosPantallaFinal` (sin globales)

### Secuencias y Demos
- `iniciarSecuenciaDemo()` / `detenerSecuenciaDemo()`: patrones no bloqueantes 2s
- Demostración automática de salidas

### Historial y Logging
- `registrarHistorial()`: logging a microSD con `TipoRegistroHistorial`
- `emitirEventoHistorial()`: salida serial para depuración

## 📁 Estructura del Repo

```
PROJECT DOMUS/
├── firmware/              # Producto principal + diagnósticos
├── obsidian/              # Notas 00-68 + bitácora completa
├── audio/                 # 168 MP3 (Carlos/Karla 01-21) + MANIFEST.csv
├── visualizaciones/       # Diagramas SVG
├── docs/                  # Documentos de entrega
├── tools/                 # Validadores y generadores
└── output/                # Exportaciones generadas
```

## 🚀 Empezar

1. Leer [el inicio de la bóveda](obsidian/proyect%20domus/00%20-%20Inicio.md) y el [índice de rutas](docs/INDICE.md)
2. Para el banco actual, abrir [firmware/domus_esqueleto](firmware/domus_esqueleto/README.md)
3. Seguir la [prueba de hoy](firmware/PRUEBA_HOY.md) y el [diagrama vigente del banco](visualizaciones/domus-banco-final-s8050-ir.svg)
4. Para capturar códigos IR y calibraciones sin activar salidas, usar `firmware/diagnosticos/domus_banco_integracion/`

## 🛠️ Validación Local

```powershell
python tools/validate_project.py
python tools/validate_markdown.py
python tools/check_perfil_matrix.py
```

La compilación de producto para ESP32-S3 usa Arduino-ESP32 3.3.10 y las bibliotecas declaradas en la [guía de firmware](firmware/casa_inteligente_v4/README.md).

## 📊 Tests

| Test Suite | Resultados |
|---|---|
| `test_firmware_contract.py` | 27/27 ⭐ |
| `test_hil_producto_contract.py` | 4/4 ⭐ |
| `test_jarvis_audio.py` | 13/13 ⭐ |
| `test_pantalla_final.py` | 5/5 ⭐ (incluye nativo g++) |
| `test_casa_candidato.py` + `test_native_safety.py` | nativos en verde |
| `test_native_firmware.py` | nativos en verde |
| **Total** | **49+ passed, 0 failed** 🟢 |

_(8 skipped = HIL, requiere placa física conectada)_

**Mejoras 2026-09-18**: `PINTEST_ALL` añadido, DIAGNOSTICO con ADC crudos y calibración, optimización de automatizaciones combinadas (una sola lectura DHT), corrección de indentación en `domus_pantalla.h`, corrección de import en `test_pantalla_final.py`.

## 📄 Licencia

Este proyecto está bajo la licencia **MIT**. Ver el archivo [LICENSE](LICENSE) para más detalles.

---

<center>⟳ <sub>Proyecto DOMUS - firmware completado sin comprar nuevo hardware</sub> ⟳</center>