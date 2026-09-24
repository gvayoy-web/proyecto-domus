# Conexiones — Programa TODO (`firmware/domus_todo/domus_todo.ino`)

**Placa:** ESP32-S3 N16R8 (COM9). **Perfil:** todo-en-uno sin 74HC595.
**Firmware válido:** solo `domus_todo.ino`. No usar con el firmware de banco
(los GPIO5/6/11/18 cambian de función).

---

## 1. Alimentación y protección (PB-JARVIS)

```
Fuente 5V (+) ──► [portafusible 2A] ──► [interruptor] ──► barra 5V_BUS
Fuente 5V (−) ──► barra GND ──► cable negro grueso ──► GND ESP32 / PB-CASA
```

- Fusible **2A en serie con el +5V, a la entrada**, antes de repartir.
- Un solo GND común para fuente, ESP32, DRV8833, DFPlayer y sensores.
- ESP32 por **USB** en banco. El puente 5V_BUS → VIN **solo** en feria sin USB
  (nunca USB + fuente externa a la vez).
- Medir el adaptador (V, polaridad, A) antes de usarlo. MB102 solo pruebas
  ligeras. Batería 9V no se usa.

## 2. DRV8833 en modo 1-pin (bomba + ventilador)

AIN2 y BIN2 **amarrados a GND en hardware**: HIGH = adelante, LOW = costa.
Así alcanzan los pines para todo lo demás.

```
ESP32 GPIO4 ──► IN1 (= AIN1) ──┐
GND         ──► IN2 (= AIN2) ──┴── canal A ──► OUT1/OUT2 ──► bomba + 1N4007
ESP32 GPIO7 ──► IN3 (= BIN1) ──┐
GND         ──► IN4 (= BIN2) ──┴── canal B ──► OUT3/OUT4 ──► ventilador + 1N4007
nSLEEP ──► 3V3 (siempre despierto)
VCC logica ──► 3V3
VM ──► 5V_BUS (despues del fusible)
GND ──► GND comun
```

- 1N4007 en paralelo a cada motor, **raya hacia el (+)**.
- Bomba solo sumergida. Corte de seguridad a 120 s (firmware).
- Pin "ult" del módulo: **al aire hasta identificarlo**. Si dice VCC → 3V3;
  si dice FAULT/FLT → al aire (solo lectura futura).

## 3. Luces directas (sin 74HC595)

| GPIO | LED | Cableado |
|---|---|---|
| 5 | sala | GPIO5 ─ R220Ω ─ ánodo, cátodo ─ GND |
| 6 | cuarto | GPIO6 ─ R220Ω ─ ánodo, cátodo ─ GND |
| 8 | cultivo (invernadero) | GPIO8 ─ R220Ω ─ ánodo, cátodo ─ GND |

## 4. Sonido DFPlayer Mini (Serial1 9600 8N1)

| ESP32 | DFPlayer |
|---|---|
| GPIO11 (RX) | TX del DFPlayer (directo) |
| GPIO18 (TX) | RX del DFPlayer (con **R 1k en serie**) |
| — | VCC → 5V_BUS, GND → GND común |
| — | SPK_1/SPK_2 → parlante (pendiente si no hay) |
| — | microSD: carpetas `01`–`21` (Carlos) y `51`–`71` (Karla), pistas `001`–`004` en cada una (`audio/jarvis_sd/` tiene las 168) |

Sin BUSY conectado el sketch dispara y reporta por Serial (`DFP>carpeta X pista Y`).
Sin parlante/SD igual funciona todo menos el audio físico.

## 5. Sensores (todos a 3V3, nunca 5V en un GPIO)

| Sensor | Conexión |
|---|---|
| Suelo (módulo) | AO → GPIO15, VCC → 3V3, GND → GND. DO libre. Sonda al módulo |
| Nivel (placa roja) | SIG → GPIO16, VCC → 3V3, GND → GND (seguir `S/+/-`) |
| LDR | 3V3 ─ R10k ─┬─ GPIO3 ; ┬─ LDR ─ GND |
| DHT11 | pata1 → 3V3, pata2 → GPIO14 **+ 10k entre pata1 y pata2**, pata3 aire, pata4 → GND |
| PIR | OUT → GPIO9, VCC → 3V3, GND → GND |

## 6. UI: LCD, IR, PARO

| Módulo | Conexión |
|---|---|
| LCD 1602 | SDA → GPIO17, SCL → GPIO13, VCC → **3V3** (nota 39), GND → GND. SDA/SCL **antes** de encender |
| IR VS1838B | OUT → GPIO12, VCC → 3V3, GND → GND |
| Botón PARO | GPIO10 ─┤├── GND (INPUT_PULLUP) |
| Botones MODO/SILENCIO | **DESCONECTADOS** (GPIO11/18 ahora son la UART del DFPlayer; en el mando de producto: CH = Spare, PLAY = silencio; en `domus_todo` aún CH = página LCD) |

## 7. Croquis completo

```
                     ┌────────────────── ESP32-S3 ──────────────────┐
                     │  15◄suelo 16◄nivel 3◄LDR 14◄DHT 9◄PIR        │
  USB (banco) ──────►│  4►bomba 7►vent 5►sala 6►cuarto 8►cultivo    │
                     │  11◄►DFP-TX 18◄►DFP-RX 12◄IR 10◄PARO        │
                     │  17◄►LCD-SDA 13◄►LCD-SCL 3V3/GND sensores   │
                     └──────┬───────────────┬──────────────┬───────┘
                            │               │              │
                    ┌───────▼───────┐ ┌─────▼──────┐ ┌────▼─────┐
                    │   DRV8833     │ │  DFPlayer  │ │ 3 LEDs   │
                    │ IN1=4 IN2=GND │ │ TX>11 RX<18│ │ 5/6/8    │
                    │ IN3=7 IN4=GND │ │ VCC 5V     │ │ +220Ω    │
                    │ nSLEEP=VCC VM │ │ GND SPK SD │ │          │
                    │ =5V_BUS(fus.) │ │            │ │          │
                    └───────┬───────┘ └────────────┘ └──────────┘
                            │
                    ┌───────▼────────────────┐
                    │ bomba + 1N4007 (A)     │
                    │ ventilador + 1N4007(B) │
                    └────────────────────────┘
```

## 8. Jarvis: 2 LED azules vivos + parlante + LCD + mando

**LED HABLA (habla de verdad, sin GPIO):** el pin BUSY del DFPlayer queda en
LOW mientras reproduce. Va directo al LED con un S8050:

```
5V_BUS ──► R220Ω ──► anodo LED HABLA, catodo ──► colector S8050
emisor S8050 ──► GND ; base S8050 ──► R10k ──► pin BUSY del DFPlayer
```

**LED VIVO (respira, via GPIO9; PIR ignorado en este programa):**

```
5V_BUS ──► R220Ω ──► anodo LED VIVO, catodo ──► colector S8050
emisor S8050 ──► GND ; base S8050 ──► R1k ──► GPIO9 (PWM 1 kHz)
```

Comportamiento (firmware): reposo = respiración ~3 s; al hablar o tocar el
mando = parpadeo excitado 4 s / 1.5 s; PARO = parpadeo rápido. Si tu DFPlayer
no trae pin BUSY, une el cátodo del LED HABLA al mismo transistor del VIVO
(modo degradado: ambos respiran).

**Parlante:** SPK_1/SPK_2 del DFPlayer directo al parlante (vale 4/6/8 Ω;
a más Ω menos volumen; el ampli da ~3 W). **LCD:** los mismos 4 pines de
siempre (GND/3V3/SDA17/SCL13). **Control:** receptor IR (OUT12/3V3/GND) +
mando CAR MP3 con las 21 teclas de la nota 64.

## 9. Orden de prueba (Serial 115200)

1. `ESTADO` → todo apagado, sensores con valor.
2. `SUELO` / `NIVEL` → calibrar: `CAL_SECO`, `CAL_HUMEDO`, `CAL_NIVEL`.
3. `LUZ1_ON` / `LUZ1_OFF` (igual 2 y C) → LEDs.
4. `SONAR` → debe salir `DFP>carpeta 21 pista 1` (y sonar si hay SD+parlante).
5. Bomba **sumergida**: `RIEGO_ON` / `RIEGO_OFF`. Mando: **5** bomba, **4** vent,
   **1/2/3** luces, **8** suelo+agua, **9/EQ** diagnóstico, **0** todo off,
   **CH+/CH−** auto/manual; **CH** = página LCD en este sketch, Spare en producto; **PLAY** silencio.
