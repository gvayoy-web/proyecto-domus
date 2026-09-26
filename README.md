# PROJECT DOMUS
## Casa Inteligente Local para Maqueta ESP32-S3

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-ff69b4.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Arduino%20ESP32-3.3.10-004422.svg)](https://github.com/espressif/arduino-esp32)
[![Tests](https://img.shields.io/badge/Tests-107_passed_0_failed-brightgreen.svg)](firmware/tests)
[![Profile](https://img.shields.io/badge/Profile-CASA_FINAL_perfil_4-green.svg)](obsidian/proyect%20domus/71%20-%20Perfil%20CASA_FINAL_DRV8833_DFPLAYER.md)
[![Version](https://img.shields.io/badge/Version-v4.0-4A90E2.svg)](https://github.com/Isaac/casa_inteligente_v4/commits/proyecdomus)
[![Lines](https://img.shields.io/badge/Code-1.9K-orange.svg)](firmware/casa_inteligente_v4/casa_inteligente_v4.ino)
[![Obsidian](https://img.shields.io/badge/Docs-Obsidian-9944FF.svg)](obsidian/proyect%20domus)

---

**PROJECT DOMUS** es una casa inteligente local (sin nube, sin internet) para una
maqueta con **ESP32-S3 N16R8** (16 MB Flash / 8 MB PSRAM OPI). Controla riego,
iluminación y escenas mediante botones, mando IR y una consola de PC con voz —
todo pensado para funcionar igual con o sin conexión. El proyecto se cerró y
**presentó al jurado el 26 de septiembre de 2026** (ver [nota 82](obsidian/proyect%20domus/82%20-%20Cierre%20jurado%20bomba%20GPIO17%20Jarvis%20PC%20y%20documentacion.md)).

## 🧠 Cómo funciona

```
   Mando IR (GPIO12)   Botones (PARO/SILENCIO)   Consola PC (USB serie)
          │                    │                         │
          └────────────┬───────┴────────────┬────────────┘
                       ▼                    ▼
              ┌─────────────────────────────────────┐
              │  ESP32-S3 · casa_inteligente_v4.ino │
              │  perfil 4 · modo AUTO = invernadero │
              └─────────────────────────────────────┘
                │        │        │        │
             Bomba     LEDs     Suelo     LDR
            GPIO17    5/8/6    GPIO15    GPIO3
```

- **Riego**: bomba por GPIO directo (GPIO ALTO = ON) gobernada por humedad de
  suelo con histéresis (seco ≥ 4000 → ON, ≤ 3000 → OFF), timeout de 120 s y
  `RIEGO_ON` manual. Encender la bomba apaga las luces (`BOMBA_ON_APAGA_LUCES`).
- **Modo AUTO = invernadero**: lee suelo/LDR cada 5 s y riega solo si hace
  falta; las luces se apagan tras una transición de 3 s. Modo MANUAL = show
  de luces por IR.
- **PARO de emergencia** (botón o `PARO`): corta todo, guarda el estado y exige
  `REARMAR`. El watchdog y el modo seguro se autorecuperan.
- **Seguridad**: mapa de pines con `static_assert`, pines únicos por
  `MAPA_CASA`, límites de serial, salidas activas en HIGH (cableado "+"
  → GPIO, `DOMUS_SALIDAS_ECONOMICAS=1`).

## 🖥️ Consola Jarvis/NEXUS (`tools/jarvis_pc/`)

`jarvis.html` es la interfaz de PC presentada al jurado: se conecta por WebUSB
al puerto serie y ofrece HUD con chips de estado, subtítulos y **voz TTS en
español** (elige la mejor voz neural del PC; clic en "Voz" para cambiarla),
**diagnóstico hablado** (Diagnóstico / Prueba guiada / Recuperar), teclado
IR emulado y **SFX estéreo** (blips, chime de conexión, alarma de PARO) por
las bocinas del PC. Sin dependencias ni servidores: basta abrirlo en Edge/Chrome.

```powershell
node tools/jarvis_pc/smoke_jarvis.js   # 64 checks, 0 fallos
```

## 📦 Estado final

| Métrica | Valor |
|---|---|
| **Tests Python** | 107 passed, 0 failed, 1 skipped ✅ |
| **Smoke HTML** | 64/64 ✅ (`tools/jarvis_pc/smoke_jarvis.js`) |
| **Firmware en COM9** | Carga HIL 8/8; bomba GPIO17 verificada en vivo |
| **Perfil** | `CASA_FINAL_DRV8833_DFPLAYER` (perfil 4) |
| **Hardware real** | Bomba GPIO17 directa; LEDs 5/8/6; suelo 15; LDR 3; IR 12 |
| **Descartados** | LCD y DHT11 (quemados), ventilador, reconocimiento de voz |
| **Deshabilitado** | DFPlayer (su RX era GPIO17, hoy bomba) — reactivar exige recablear |
| **Calibración** | `PROVISIONAL` — afinar con `CAL_SECO`/`CAL_HUMEDO` + `CAL_GUARDAR` |

> **Nota 82**: cierre — bomba GPIO17, panel de diagnóstico hablado, voz y SFX,
> documentación final. Ver [[82 - Cierre jurado bomba GPIO17 Jarvis PC y documentacion]].
> Bitácora completa en las notas [[00 - Inicio]] … [[81 - Bomba GPIO directa AUTO invernadero]].

## ✨ Características implementadas

- **Audio 1:1 por botón** (22 eventos × 4 variantes × 2 voces = 176 MP3 en
  `audio/jarvis_sd/`, Carlos 01-22 / Karla 51-72, `MANIFEST.csv` con SHA-256).
  Hoy el reproductor está deshabilitado en firmware; los MP3 y la SD siguen
  siendo entregables.
- **Automatizaciones combinadas** (calor+tierra_seca+agua, luz+presencia),
  auto-off de bomba (5 min) y luces (10 min), estadísticas sin bloqueos
  (`EstadisticasDOMUS`).
- **21 teclas IR** con aprendizaje real y códigos únicos; tecla `CH` alterna
  la salida Spare (GPIO6), `PLAY` silencio, `CH-`/`CH+` modo manual/automático.
- **Historial y logging** a microSD (`registrarHistorial`) y eventos seriales
  (`DIAGNOSTICO`, `ESTADO`, `ACK/NACK`, `PRUEBA`, `PINTEST`).

## 📁 Estructura del repo

```
casa_inteligente_v4/
├── firmware/                  # Producto (casa_inteligente_v4/), diagnósticos, tests
├── tools/jarvis_pc/           # Consola de PC: jarvis.html + smoke (64 checks)
├── obsidian/proyect domus/    # Bóveda 00-82: bitácora y autoridad documental
├── audio/                     # 176 MP3 (jarvis_sd 01-22/51-72) + MANIFEST.csv
├── hardware/                  # Guías de montaje y planos canónicos
├── visualizaciones/           # Diagramas SVG del banco
├── docs/                      # ENTREGA_FINAL, ESTADO_ACTUAL, INDICE
└── output/                    # Exportaciones generadas
```

## 🚀 Empezar

1. Leer [Inicio](obsidian/proyect%20domus/00%20-%20Inicio.md) y el
   [índice de rutas](docs/INDICE.md); el cierre está en la
   [nota 82](obsidian/proyect%20domus/82%20-%20Cierre%20jurado%20bomba%20GPIO17%20Jarvis%20PC%20y%20documentacion.md).
2. Cargar el firmware: `firmware/casa_inteligente_v4/` con FQBN
   `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CPUFreq=240,LoopCore=1`
   (última carga en COM9).
3. Abrir `tools/jarvis_pc/jarvis.html` en Edge/Chrome y conectar el puerto.
4. Para capturar códigos IR o calibrar sin accionar salidas:
   `firmware/diagnosticos/domus_banco_integracion/`.

## 🛠️ Validación local

```powershell
python tools/validate_project.py
python tools/validate_markdown.py
python -m unittest discover -s firmware/tests    # 107 passed
node tools/jarvis_pc/smoke_jarvis.js             # 64 passed
```

| Suite | Resultados |
|---|---|
| `test_firmware_contract.py` | 28/28 |
| `test_jarvis_audio.py` | 13/13 |
| `test_domus_esqueleto_contract.py` | 19/19 |
| `test_casa_candidato.py` | 10/10 |
| `test_esqueleto_sim.py` | 8/8 |
| `test_driver_backends.py` | 7/7 |
| `test_pantalla_final.py` | 5/5 (incluye nativo g++) |
| `test_banco_integracion.py` + `test_hil_producto_contract.py` | 4/4 c/u |
| Resto (nativos, simulación, HIL, campaña) | 9 |
| **Total Python** | **107 passed, 0 failed, 1 skipped** 🟢 |

_(El HIL con placa hace SKIP si no hay puerto; el último corrido en COM9 fue 8/8.
La compilación nativa g++ de esta máquina quedó dañada en el entorno
(WinGet GCC 16.1.0, cc1plus inestable); las suites nativas corren en CI Ubuntu.)_

## 📄 Licencia

Este proyecto está bajo la licencia **MIT**. Ver el archivo [LICENSE](LICENSE) para más detalles.

---

<center>⟳ <sub>PROJECT DOMUS — casa inteligente local, cerrada y presentada al jurado · v4.0</sub> ⟳</center>
