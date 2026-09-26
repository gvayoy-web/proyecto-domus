---
estado: vigente
fecha: 2026-09-21
autoridad: 74
tipo: hardware-gpio-v5
---

# 74 - Hardware final y mapa GPIO v5

> [!IMPORTANT]
> Esta nota reemplaza la nota 69 como referencia de hardware. Actualizada
> despues de eliminar ventilador (quemado), reorganizar salidas y recablear
> la bomba a **GPIO17** (nota 82). El LCD se quemo y quedo descartado (nota 80).

## Mapa GPIO definitivo (lado izquierdo, GPIO 3-18)

Solo el lado izquierdo del devkit ESP32-S3 N16R8 es usable. El GPIO11 no
existe en la placa; los demas estan asignados o reservados.

| GPIO | Funcion | Componente | Direccion |
|------|---------|------------|-----------|
| 3 | LDR | Divisor voltaje con 10kohm | Entrada analogica |
| 4 | DRV AIN1 | Reserva (la bomba salio del DRV) | Fuera del camino |
| 5 | Casa | 2 LEDs paralelo (sala+ducha) + 330ohm | Salida, HIGH = ON |
| 6 | Spare | LED Jarvis + 330ohm | Salida, HIGH = ON |
| 7 | Cultivo | Sin etapa fisica | Bloqueado por software |
| 8 | Porche | 1 LED + 330ohm | Salida, HIGH = ON |
| 9 | SILENCIO | Boton MIC OFF → GND | Entrada INPUT_PULLUP |
| 10 | PARO | Boton → GND | Entrada INPUT_PULLUP |
| 11 | — | No existe en la placa | - |
| 12 | IR | VS1838B receptor (GPIO12 reservado) | Entrada |
| 13 | I2C SCL | Reserva (LCD descartado) | Bidireccional |
| 14 | DHT11 | DATA + 10kohm pull-up a 3V3 | Entrada |
| 15 | Suelo | Sensor resistivo AO | Entrada analogica |
| 16 | Nivel | Reserva (sin sensor en inventario) | Sin leer |
| 17 | Bomba | S8050 + mini bomba DC, directa | Salida, HIGH = ON |
| 18 | DEMO | Boton a GND (compartido con TX DFPlayer) | Entrada pull-up |

## Componentes que se usan

| # | Componente | Cantidad | Donde va | Estado |
|---|------------|----------|----------|--------|
| 1 | ESP32-S3 N16R8 | 1 | Cerebro | OK |
| 2 | LCD1602 + backpack I2C | 1 | I2C (SDA/SCL) | **Quemado, descartado** (nota 80) |
| 3 | DFPlayer Mini + microSD | 1 | RX era GPIO17, TX GPIO18 | **Deshabilitado** (RX = bomba hoy) |
| 4 | Parlante 4ohm 3W | 1 | DFPlayer SPK | Pendiente |
| 5 | Parlante 8ohm 1W | 1 | DFPlayer SPK (serie) | Pendiente |
| 6 | VS1838B receptor IR | 1 | GPIO12 (reservado) | OK |
| 7 | Mando IR 21 teclas | 1 | Control remoto | OK |
| 8 | DHT11 | 1 | GPIO14 | Cableado, descartado (quemado) |
| 9 | Sensor suelo resistivo | 1 | GPIO15 | OK |
| 10 | Sensor nivel agua | 1 | GPIO16 (reserva) | Sin sensor en inventario |
| 11 | LDR | 1 | GPIO3 | OK |
| 12 | Mini bomba DC 3-6V | 1 | GPIO17 directo via S8050 | **Probada en vivo** (nota 82) |
| 13 | S8050 transistor | 1 | Etapa de la bomba | OK |
| 14 | LED sala | 1 | GPIO5 + 330ohm | Paralelo con ducha |
| 15 | LED ducha | 1 | GPIO5 + 330ohm | Paralelo con sala |
| 16 | LED porche | 1 | GPIO8 + 330ohm | OK |
| 17 | LED cultivo | 3 | GPIO7 (etapa no instalada) | Bloqueado por software |
| 18 | LED VIVO (breathing) | 1 | — (GPIO9 es hoy SILENCIO) | Eliminado |
| 19 | Boton PARO | 1 | GPIO10 → GND | INPUT_PULLUP |
| 20 | 1N4007 diodo | 1 | Paralelo bomba | Flyback |
| 21 | Fuente 5V/2A USB-C | 1 | Cargas | **COMPRAR** |
| 22 | microSD 4-32GB FAT32 | 1 | DFPlayer (deshabilitado) | Reserva |

## Resistencias necesarias

| Valor | Cantidad | Uso |
|-------|----------|-----|
| 330ohm | 2 | LED sala+ducha (paralelo en GPIO5), LED porche |
| 100ohm | 1 | 3xLED cultivo paralelo (si se instala etapa) |
| 1kohm | 1 | Base S8050 de la bomba |
| 10kohm | 3 | Pull-up DHT11, divisor LDR, bias S8050 |

## Alimentacion dual

`
USB-C → ESP32 (logica, LCD, sensores, Serial)
5V/2A → Bomba, DFPlayer, LEDs (cargas)
GND comun entre ambas fuentes
`

Consumo estimado: bomba 0.5-0.6A + DFPlayer 0.2A + LEDs 0.1A + ESP32 0.15A = ~1.05A max.
Fuente 2A sobra con margen.

## NO necesarios (dejar en casa)

Raspberry Pi Pico, ESP8266, OLED, displays, keypad, joystick, MPU6050, HC-SR04,
RFID, servo, relay, L293D, 74HC595, buzzers, TP4056, cable 9V, WS2812, INMP441,
MAX98357A, MAX98306 (necesita microUSB, no sirve), 74AHCT125, potenciometros,
switches, reed, termistor, tilt switch, ventilador (quemado).

## Relacionadas

- [[69 - Placa real lado usable y restriccion de GPIO]]
- [[01 - Inventario confirmado]]
- [[73 - Auditoria Profunda Completa del Proyecto]]
- [[75 - Audio Jarvis remodelado - Carlos Karla y SD]]
- [[76 - Control IR y modo inteligente]]