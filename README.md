# PROJECT DOMUS
## Casa Inteligente Local para Maqueta ESP32-S3

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-ff69b4.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Arduino%20ESP32-3.3.10-004422.svg)](https://github.com/espressif/arduino-esp32)
[![Tests](https://img.shields.io/badge/Tests-107_passed_0_failed-brightgreen.svg)](firmware/tests)
[![Profile](https://img.shields.io/badge/Profile-CASA_FINAL_perfil_4-green.svg)](obsidian/proyect%20domus/71%20-%20Perfil%20CASA_FINAL_DRV8833_DFPLAYER.md)
[![Version](https://img.shields.io/badge/Version-v4.0-4A90E2.svg)](https://github.com/gvayoy-web/proyecto-domus/commits/main)
[![Docs](https://img.shields.io/badge/Docs-Obsidian%2000--82-9944FF.svg)](obsidian/proyect%20domus)

---

**PROJECT DOMUS** es una casa inteligente **local** (sin nube, sin internet) para
una maqueta con **ESP32-S3 N16R8** (16 MB Flash / 8 MB PSRAM OPI). Controla riego,
iluminación y escenas con botones, mando IR y una consola de PC con voz. El
proyecto está **cerrado y presentado al jurado el 26 de septiembre de 2026**
(ver [nota 82](obsidian/proyect%20domus/82%20-%20Cierre%20jurado%20bomba%20GPIO17%20Jarvis%20PC%20y%20documentacion.md)).

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

- **Riego**: bomba por GPIO directo (GPIO ALTO = ON) con histéresis de suelo
  (seco ≥ 4000 → ON, ≤ 3000 → OFF), timeout de 120 s y `RIEGO_ON` manual.
  Encender la bomba apaga las luces (`BOMBA_ON_APAGA_LUCES`).
- **Modo AUTO = invernadero**: lee suelo/LDR cada 5 s, riega solo si hace falta
  y apaga las luces tras una transición de 3 s. Modo MANUAL = show de luces.
- **PARO de emergencia** (botón o serial): corta todo, exige `REARMAR`;
  watchdog y modo seguro se autorecuperan.
- **Seguridad**: mapa de pines con `static_assert`, pines únicos, límites de
  serial, salidas activas en HIGH (cableado "+" → GPIO).

---

## 🛠️ Construye tu propio DOMUS

### Paso 0 — Materiales

| Grupo | Piezas clave |
|---|---|
| Cerebro | ESP32-S3 N16R8 (16MB/8MB) |
| Riego | Mini bomba DC 3–6 V + S8050 + 1 kΩ + 1N4007 (flyback) |
| Luces | 2 LEDs azules (Casa/Porche) + 1 LED (Spare) + 330 Ω |
| Sensores | LDR + divisor 10 kΩ (GPIO3), sensor de suelo resistivo (GPIO15), DHT11 (GPIO14, descartado por quemado) |
| Control | Receptor VS1838B/HX1838 + mando 21 teclas (GPIO12), botones PARO (GPIO10) y SILENCIO (GPIO9) a GND |
| Fuente | USB-C para lógica + 5 V/2 A para cargas, GND común |

- Cotización y compra vigentes: notas [[41 - Compra nacional minima y banco de soldadura]] y [[42 - Solicitud final de cotizacion C&D]].
  La nota 03 es **histórica: no comprar desde ahí**.
- Lista de materiales de la maqueta: `hardware/planos/materiales_ultimate.csv`.
- **No necesarios**: LCD 1602 (quemado), DFPlayer (deshabilitado: su RX era
  GPIO17, hoy bomba), ventilador, Raspberry Pi Pico, relés, WS2812, MAX98306…

### Paso 1 — Planos de la maqueta (últimos)

Todo está en [`hardware/planos/`](hardware/planos):

| Archivo | Qué es |
|---|---|
| `PLANOS_ULTIMATE_CONSTRUCCION_MAQUETA.pdf` | Planos de construcción con cotas (fuente de verdad) |
| `GUIA_MONTAJE_ULTIMATE.md` | Guía de fabricación paso a paso (corte, perforación, armado) |
| `PROJECT_DOMUS_PLANOS_ULTIMATE_CONSTRUCCION.zip` | Paquete completo de láminas |
| `lista_corte_ultimate.csv` · `perforaciones_y_accesos.csv` · `piezas_modelo.csv` | Listas de corte y medidas |
| `project_domus.obj` / `.mtl` + renders PNG | Modelo 3D y vistas |
| `generate_ultimate_plans.py` | Regenerador de los planos |

También: [`hardware/GUIA_MONTAJE.md`](hardware/GUIA_MONTAJE.md) (armado crudo)
y [`hardware/planos/GUIA_MONTAJE_ULTIMATE.md`](hardware/planos/GUIA_MONTAJE_ULTIMATE.md).
Medida rectora: **base 800 × 520 mm**; ante cualquier duda **mandan las cotas
del PDF y los CSV**, no los renders.

### Paso 2 — Cableado pin por pin

1. **Guía completa**: nota [[78 - Guia de Conexiones Completa v5]] — paso a paso
   sin experiencia previa; versión visual en
   [`hardware/Conexion_Domus_Guia_Completa.html`](hardware/Conexion_Domus_Guia_Completa.html).
2. **Mapa GPIO definitivo**: nota [[74 - Hardware final y mapa GPIO v5]].
3. **Manual técnico**: nota [[18 - Manual maestro de conexiones pin por pin]]
   (tabla pin por pin, alimentación dual, botones, bomba S8050).
4. **Diagrama vigente del banco**:
   [`visualizaciones/domus-banco-final-s8050-ir.svg`](visualizaciones/domus-banco-final-s8050-ir.svg)
   e interactivo en [`visualizaciones/diagrama-cableado-interactivo/`](visualizaciones/diagrama-cableado-interactivo).

Reglas de oro:

- Solo el **lado izquierdo** del devkit es usable; **GPIO11 no existe** en la placa.
- **GPIO12 está reservado** al receptor IR: no usarlo para nada más.
- **Nunca 5 V a un GPIO**; PARO va a GND con `INPUT_PULLUP` y debe quedar accesible.
- Bomba: GPIO17 → 1 kΩ → base S8050, emisor a GND, 1N4007 en paralelo con el motor.

### Paso 3 — Compilar y cargar el firmware

```powershell
# 1. Arduino-CLI con el core de Espressif (Arduino-ESP32 3.3.10)
arduino-cli core install esp32:esp32
arduino-cli lib install "LiquidCrystal I2C"     # solo por el #include (LCD fuera de circuito)

# 2. Compilar (perfil 4 = CASA_FINAL, por defecto en el .ino)
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CPUFreq=240,LoopCore=1" firmware/casa_inteligente_v4

# 3. Cargar (sustituye COM9 por tu puerto; `arduino-cli board list`)
arduino-cli upload -p COM9 --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CPUFreq=240,LoopCore=1" firmware/casa_inteligente_v4

# 4. Monitor serie
arduino-cli monitor -p COM9 -c baudrate=115200
```

- Sketch de producto: [`firmware/casa_inteligente_v4/casa_inteligente_v4.ino`](firmware/casa_inteligente_v4/casa_inteligente_v4.ino)
  (perfil `DOMUS_PERFIL_CASA=4`). Alternativa: el wrapper
  [`firmware/domus_esqueleto/domus_esqueleto.ino`](firmware/domus_esqueleto), que
  define el perfil e incluye el producto literalmente.
- Guía del firmware y hardware real: [`firmware/casa_inteligente_v4/README.md`](firmware/casa_inteligente_v4/README.md).
- Evidencia de compilación: [`firmware/COMPILACION_VALIDADA.md`](firmware/COMPILACION_VALIDADA.md)
  (434.341 bytes, RAM 8–14 %).

### Paso 4 — Primeras pruebas (seguras)

Lee primero [`firmware/PRUEBA_HOY.md`](firmware/PRUEBA_HOY.md).

1. **La bomba arranca apagada y bloqueada** (`MANUAL_OFF`): ninguna lectura
   provisional la enciende al conectar.
2. Monitor a **115200**: envía `ESTADO`, luego `PRUEBA` (copia
   `PRUEBA;INICIO`…`PRUEBA;FIN`), `DIAGNOSTICO`, `PINTEST_ALL`.
3. **Bomba**: métela en agua y envía `RIEGO_ON` / `RIEGO_OFF`; el timeout de
   120 s la corta sola. `RIEGO_AUTO` usa histéresis de suelo.
4. **PARO/rearme**: `PARO` corta todo; `REARMAR` lo devuelve.
5. **Sensores y códigos IR sin accionar salidas**: carga
   [`firmware/diagnosticos/domus_banco_integracion`](firmware/diagnosticos/domus_banco_integracion)
   (`IR_LISTA`, `MUESTRA_SECO`, `MUESTRA_HUMEDO`, `MUESTRA_OSCURO`, `MUESTRA_CLARO`).
6. **Calibración**: con esas muestras, define `CAL_SECO`/`CAL_HUMEDO`
   (y `CAL_LDR_*`) en el firmware y guarda con `CAL_GUARDAR` (con PARO activo).
   Hoy `CALIBRACION=PROVISIONAL`.
7. **Aprender el mando**: `IR_GRABAR_0` … `IR_GRABAR_20`, revisa con `IR_LISTA`
   (`APRENDIDAS=n/21`). Detalles en el [README del firmware](firmware/casa_inteligente_v4/README.md).
8. **HIL automatizado** (no flasha ni enciende la bomba):

```powershell
python -m pip install -r firmware/tests/requirements-hil.txt
$env:DOMUS_PORT = "COM9"
python -m unittest firmware.tests.test_hil_producto -v    # último corrido: 8/8
```

### Paso 5 — Consola Jarvis en tu PC

Abre [`tools/jarvis_pc/jarvis.html`](tools/jarvis_pc/jarvis.html) en Edge o
Chrome (WebUSB, 100 % local), conecta el puerto y recarga con `Ctrl+Shift+R`:

- HUD con chips de estado, **subtítulos y voz TTS en español** (clic en "Voz"
  para cambiar de voz), teclado IR emulado.
- Panel **Diagnóstico hablado**: Diagnóstico / Prueba guiada / Recuperar —
  el equipo contesta en voz alta.
- **SFX estéreo** por las bocinas del PC: blip al pulsar, chime al conectar,
  alarma en PARO.

```powershell
node tools/jarvis_pc/smoke_jarvis.js    # 64 checks, 0 fallos
```

### Paso 6 — Operar

- Teclas IR: **CH+** modo automático (invernadero), **CH−** manual, **CH**
  alterna Spare, **0** apaga todo, **200+** rearma, **100+** alterna voz.
- Automatizaciones: calor+tierra_seca+agua, luz+presencia, auto-off de bomba
  (5 min) y luces (10 min).

---

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

## ✨ Características implementadas

- **Audio 1:1 por botón**: 176 MP3 en `audio/jarvis_sd/` (22 eventos × 4
  variantes × 2 voces, Carlos 01-22 / Karla 51-72) con `MANIFEST.csv` SHA-256.
  El reproductor DFPlayer está deshabilitado en hardware; los MP3 siguen siendo
  entregables (SD FAT32 en la raíz).
- **Automatizaciones combinadas** y estadísticas sin bloqueos
  (`EstadisticasDOMUS`): encendidos, riegos, emergencias, errores.
- **21 teclas IR** con aprendizaje real y códigos únicos (rechaza duplicados).
- **Historial y logging** a microSD (`registrarHistorial`) y eventos seriales
  (`DIAGNOSTICO`, `ESTADO`, `ACK/NACK`, `PRUEBA`, `PINTEST`).

## 📁 Estructura del repo

```
casa_inteligente_v4/
├── firmware/                  # Producto (casa_inteligente_v4/), diagnósticos, tests
├── tools/jarvis_pc/           # Consola de PC: jarvis.html + smoke (64 checks)
├── obsidian/proyect domus/    # Bóveda 00-82: bitácora y autoridad documental
├── audio/                     # 176 MP3 (jarvis_sd 01-22/51-72) + MANIFEST.csv
├── hardware/                  # Guías de montaje + planos (PDF/CSV/3D)
├── visualizaciones/           # Diagramas SVG/HTML del banco y cableado
├── docs/                      # ENTREGA_FINAL, ESTADO_ACTUAL, INDICE, PROXIMOS_PASOS
└── output/                    # Exportaciones generadas
```

## 🗺️ Dónde está cada cosa

| Quiero… | Ir a |
|---|---|
| Ver los últimos planos | `hardware/planos/` (PDF + CSV + GUIA_MONTAJE_ULTIMATE.md) |
| Cablear el hardware | Nota [[78 - Guia de Conexiones Completa v5]] + nota [[74 - Hardware final y mapa GPIO v5]] + nota [[18 - Manual maestro de conexiones pin por pin]] |
| Diagrama del banco | `visualizaciones/domus-banco-final-s8050-ir.svg` |
| Compilar/cargar firmware | Este README (Paso 3) + `firmware/casa_inteligente_v4/README.md` |
| Probar sin romper nada | `firmware/PRUEBA_HOY.md` + `firmware/diagnosticos/domus_banco_integracion/` |
| Entender el estado y alcance | [[00 - Inicio]], `docs/ESTADO_ACTUAL.md`, `docs/ENTREGA_FINAL.md` |
| Ver todo lo que falta, paso a paso | `docs/PROXIMOS_PASOS.md` |
| La bitácora completa | Notas [[00 - Inicio]] … [[82 - Cierre jurado bomba GPIO17 Jarvis PC y documentacion]] |
| Entregables antiguos | `assets/new/README.md` y `documentos/README.md` |

## 🛠️ Validación local

```powershell
python tools/validate_project.py     # lógica, decisiones, firmware y planos
python tools/validate_markdown.py    # 139 archivos, 386 enlaces
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
