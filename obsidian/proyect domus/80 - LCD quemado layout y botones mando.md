---
estado: vigente
fecha: 2026-09-23
autoridad: 80
tipo: sesion-lcd-quemado-botones-mando
depends: 76, 77, 79
---

# 80 - LCD quemado, layout físico y nuevos botones del mando

> [!IMPORTANT]
> El LCD1602 se **quemó** y queda **descartado del proyecto**.
> El firmware sigue siendo headless-tolerante (PantallaFinal con puntero nulo).
> Esta nota documenta el layout real, el estado de la sesión y los
> **nuevos botones de software** del mando/Serial.

## Hardware: qué se quema / qué cambia

| Componente | Estado |
|------------|--------|
| **LCD1602 I2C** | **QUEMADO — descartado** (no se usa, se desconecta) |
| Ventilador | Muerto; canal B DRV8833 `return false` |
| PIR | No existe; presencia = siempre true |
| MAX98306 | Inútil (DFPlayer trae amplificador) |
| Fan/spare GPIO | `spare` reasignado a **GPIO6** |

### Layout físico actual (decidido por el usuario)

| Zona | Contenido |
|------|-----------|
| **Invernadero** | Sensor de suelo + DRV8833 + bomba de agua **solos** (sin LED RGB) |
| **Casa** | 2 LEDs blancos en paralelo (GPIO5 / salida Casa) |
| **Techo (Casa)** | **DHT11 + LDR** montados en el techo |
| **Porche** | 1 LED rojo (GPIO8 / salida Porche) |
| **Jarvis** | 2 LEDs azules, 2 parlantes, DFPlayer, sensor IR |

> Sin LCD. PIR no existe. **Cultivo no tiene luz** (índice 3 siempre
> `SALIDA_FISICA=false`). **No hay sensor de nivel de agua** (GPIO16 solo
> reservado por unicidad; nunca se lee). Spare/Jarvis (índice 4) = GPIO6.

### Alimentación

- Lógica ESP32: **USB-C en COM9** (detectado de nuevo)
- Cargas: 5V/2A externo por **conector amarillo de protoboard**
- Fusible + DRV disponibles; GND común obligatorio

## Sesión: qué se hizo

1. Tests Python alineados (106 pass / 8 skip; 102 non-HIL)
2. 3 errores de compilación corregidos (GPIO dup, BIN1/BIN2, ResultadoDiagnostico)
3. Firmware compilado y subido a COM9 (commit `a2fe2a3` → origin/main)
4. RP2040 core **instalado** (`rp2040:rp2040@6.1.1`) para Raspberry Pi Pico
5. Pico aún **no detectado** en puerto COM (falta enchufarlo / micro USB)
6. LCD descartado (quemado)
7. Layout confirmado: DHT11+LDR en el techo; ESP32 de vuelta en COM9
8. Firmware con botones nuevos (CH→Spare, SPARE/TODO/DEMO Serial) compilado y **subido a COM9**
9. Diagnóstico Serial: perfil OK, Casa/Porche ON/OFF responden ACK; LCD=NINGUNA headless;
   DHT11 suspendido (3 fallos, aún sin montar en techo); LDR=-1 (sin calibrar/conectar);
   Cultivo/Spare = motor DRV / salida libre según perfil
10. **HIL falló** en `test_05_leds_ida_y_vuelta`: `INVER` (Cultivo) es canal de motor
    DRV8833 → `NACK;INVER_ON;driver_no_listo` (driver F1 no validado / canal B muerto)
11. `LED_COMMANDS` del HIL corregido a **LUZ1, LUZ2, SPARE** (luces reales del layout);
    teardown añadió `SPARE_OFF`
12. Conflicto GPIO18 resuelto: DFPlayer TX vs botón DEMO phantom `ACK;MODO_LCD` →
    lectura del botón `MAPA_CASA.demo` gateada con `if (!MP3_HABILITADO)`
13. Re-upload a COM9; **HIL 12/12 PASS** (`DOMUS_PORT=COM9`); non-HIL **107 PASS**
14. **GPIO11 no existe en la placa** (pins reales: 3–10, 12–18, 46). Remapeo:
    - SILENCIO/micOff: **GPIO11 → GPIO9** (PIR inexistente, pin libre)
    - DFPlayer UART: RX **11 → 17** (SDA del LCD quemado, liberado), TX sigue **18**
    - `transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo, …)`
    - `LCD_DESCARTADO=true`: `detectarPantalla()` no inicia `Wire` (headless)
15. Tests/README/native_integration actualizados al nuevo mapa; non-HIL **107 PASS**;
    compile + **upload COM9**; **HIL 8/8 PASS** (`DOMUS_PORT=COM9`)
16. **Corte de hardware estricto del usuario**: sin luz Cultivo, sin sonda de
    nivel; Spare/Jarvis = GPIO6; toda orden fallida habla **"No funciono"**
    (`EventoJarvis::FALLO` carpeta 22/72); `INVER_*`/`LUZC_*`/`CAL_NIVEL` fuera.

### Mapa GPIO vigente (placa real, sin GPIO11)

| Señal | GPIO | Nota |
|-------|------|------|
| LDR | 3 | techo Casa |
| Bomba (DRV AIN1) | 4 | |
| Casa (2 LEDs blancos) | 5 | |
| Spare / LEDs Jarvis (opcionales) | 6 | |
| Cultivo (DRV AIN2) | 7 | |
| Porche (LED rojo) | 8 | |
| **SILENCIO (micOff)** | **9** | antes 11 (inexistente) |
| PARO | 10 | |
| IR HX1838 | 12 | |
| SCL (LCD descartado) | 13 | no se usa I2C |
| DHT11 | 14 | techo Casa |
| Suelo | 15 | |
| GPIO16 (reservado; sin sonda) | 16 | unicidad de pines; nunca se lee |
| **SDA / DFPlayer RX** | **17** | LCD liberado |
| **MODO/demo / DFPlayer TX** | **18** | botón gateado `!MP3_HABILITADO` |
| GPIO46 | 46 | disponible; no usado |

> GPIO7 = `MAPA_CASA.cultivo` **y** `DRV8833 AIN2`; nunca se escribe como
> "luz de cultivo" (canal B del driver está muerto).

## Mando CAR MP3 — tecla CH reasignada

La tecla **CH (0x46)** ya **no** cambia de página LCD (hardware inexistente).

| Tecla | Antes | **Ahora** |
|-------|-------|-----------|
| CH 0x46 | Página LCD siguiente | **Spare ON/OFF** (salida index 4, GPIO6) |

Las teclas **ANTERIOR/SIGUIENTE** se conservan como atajos de Casa/Porche
(teclas 1/2): sin LCD no aporta navegar páginas.

Audio Jarvis de CH: carpeta **02/52 regenerada** con frases de Spare
(1=ON, 2=OFF, 3=auto ON, 4=auto OFF); `carpetaSalida(4)` → `EventoJarvis::CH`.
Tecla 3 (Cultivo sin etapa) → `NACK;SALIDA_NO_INSTALADA` + voz FALLO.

## Nuevos botones de software (Serial)

Mismo contrato ACK/NACK; entran por `COMANDOS_VALIDOS` y la tabla de relés.

| Comando | Acción |
|---------|--------|
| `SPARE_ON` / `SPARE_OFF` / `SPARE_AUTO` | Salida Spare (GPIO6) |
| `TODO_ON` | Enciende Casa+Porche+Spare (no bomba; sin Cultivo) |
| `TODO_OFF` | Apaga todas las salidas (incluye bomba) |
| `DEMO_ON` / `DEMO_OFF` | Secuencia de demostración por despachador |

**Eliminados** (hardware inexistente): `INVER_*`, `LUZC_*`, `CAL_NIVEL`,
enclavamiento de nivel y lectura `NIVEL_AGUA=`. Toda orden IR/manual
fallida anuncia **`EventoJarvis::FALLO`** (`No funciono.`, carpeta 22/72).

Sin LCD: los `pantallaFinal.mostrarMensaje(...)` son no-op seguros
(`lcd_ == nullptr`); los ACK/NACK siguen por Serial y microSD.

## Pendientes

- [ ] Enchufar Pico (micro USB) y detectar COM → sketch DHT11+LDR (si se usa como auxiliar)
- [x] Decidir ubicación física DHT11 y LDR → **techo de Casa**
- [x] ESP32: USB-C en COM9 detectado → re-upload firmware con botones nuevos
- [x] Regenerar frases CH → texto "Spare" (carpeta 02/52 + MANIFEST)
- [x] Decidir si Cultivo/Spare tienen carga física → **Cultivo sin luz; Spare = GPIO6 (2 LEDs azules Jarvis)**
- [x] Sin sensor de nivel de agua: bomba solo por suelo + timeout 120 s
- [x] Voz "No funciono" en fallos (`FALLO` = carpeta 22/72)
- [x] HIL verdes: 8/8 (`DOMUS_PORT=COM9`); non-HIL 107
- [x] Gate botón DEMO cuando `MP3_HABILITADO` (GPIO18 = DFPlayer TX)

## Relacionadas

- [[76 - Control IR y modo inteligente]]
- [[77 - Plan firmware final v5]]
- [[79 - Sesion compilacion upload COM9 y HIL]]
- [[78 - Guia de Conexiones Completa v5]]
