# Conexiones Firmware — Perfil 4: CASA_FINAL_DRV8833_DFPLAYER

**Placa:** ESP32-S3-WROOM-1 N16R8 (16MB Flash / 8MB PSRAM / 240MHz)
**USB-Serial:** CH343 en COM9
**Firmware:** `casa_inteligente_v4.ino` — perfil `DOMUS_PERFIL_CASA=4`

---

## 1. Resumen de módulos

| Módulo | Función | Bus/Pines | Estado |
|--------|---------|-----------|--------|
| DRV8833 | Driver dual H-bridge (bomba + ventilador) | GPIO4/5/6/7 + nSLEEP→VCC | Activo en perfil 4 |
| 74HC595 | Shift register de 8 bits (3 salidas LED) | GPIO8(DS), GPIO11(SHCP), GPIO18(STCP) | Activo en perfil 4 |
| DFPlayer Mini | Reproductor MP3 por UART | GPIO4(RX), GPIO7(TX) | Activo en perfil 4 (audio aplazado) |
| DHT11 | Temp/Humedad ambiental | GPIO14 | Activo |
| LCD 1602 I2C | Pantalla (dirección 0x27/0x3F) | SDA=GPIO17, SCL=GPIO13 | Activo |
| LDR | Sensor de luz ambiental | GPIO3 (divisor 10k/3V3) | Activo |
| PIR | Detector de movimiento | GPIO9 | Activo |
| Botón PARO | Paro de emergencia | GPIO10 | Activo |
| IR Receiver | Mando infrarrojo | GPIO12 | Activo |
| Relés (×5) | Salidas: bomba, sala, cuarto, ventilador, invernadero | GPIO4/5/6/7/8 | Vía DRV8833 o directo |

---

## 2. Mapa de pines del ESP32-S3 (lado usable: GPIO 3–18)

> Nota 69: El lado derecho de la placa está prohibido para GPIO. Solo GPIO 3–18 son utilizables.

### 2.1 Salidas digitales

| GPIO | Función | Módulo | Descripción |
|------|---------|--------|-------------|
| **4** | DRV8833 AIN1 | DRV8833 | Bomba — canal A entrada 1 |
| **5** | DRV8833 BIN1 | DRV8833 | Ventilador — canal B entrada 1 |
| **6** | DRV8833 BIN2 | DRV8833 | Ventilador — canal B entrada 2 |
| **7** | DRV8833 AIN2 / UART1 TX | DRV8833 + DFPlayer | Bomba — canal A entrada 2 (compartido con DFPlayer TX) |
| **8** | 74HC595 DS / Q0 (salida relé sala) | 74HC595 + Relé | Data in shift register / Salida luz sala |
| **11** | 74HC595 SHCP | 74HC595 | Shift clock (sacrifica botón SILENCIO) |
| **18** | 74HC595 STCP | 74HC595 | Storage clock (sacrifica botón MODO) |

### 2.2 Entradas y buses

| GPIO | Función | Módulo | Descripción |
|------|---------|--------|-------------|
| **3** | LDR (divisor) | LDR | Fotoresistor + R=10k entre 3V3 y GND |
| **9** | PIR | PIR | Detector de movimiento |
| **10** | PARO emergencia | Botón | Botón de paro de emergencia (activo en LOW) |
| **12** | IR Receiver | IR | Recepción de mando infrarrojo |
| **13** | I2C SCL | LCD I2C | Reloj I2C |
| **14** | DHT11 data | DHT11 | Sensor temp/humedad ambiental |
| **15** | ADC1 (suelo) | Sensor humedad | Humedad de tierra (crudo ADC) |
| **16** | ADC1 (nivel) | Sensor agua | Nivel de agua del tanque (crudo ADC) |
| **17** | I2C SDA | LCD I2C | Datos I2C |

### 2.3 Salidas analógicas (ADC1)

| Pin | Sensor | Calibración |
|-----|--------|-------------|
| GPIO15 | Suelo (humedad) | `calibracion.sueloSeco=2800`, `calibracion.sueloHumedo=1200` |
| GPIO16 | Nivel de agua | Rango 0–4095 |
| GPIO3 | LDR | Calibrado como `LUZ_PCT` (0–100%) |

---

## 3. Conexiones detalladas por módulo

### 3.1 DRV8833 (Driver de motores)

```
ESP32-S3              DRV8833
─────────             ───────
GPIO4  (AIN1) ───────► AIN1  (Bomba sentido 1)
GPIO7  (AIN2) ───────► AIN2  (Bomba sentido 2)
GPIO5  (BIN1) ───────► BIN1  (Ventilador sentido 1)
GPIO6  (BIN2) ───────► BIN2  (Ventilador sentido 2)
VCC    (3.3V) ───────► nSLEEP (Habilitado, HIGH)
GND    (GND) ───────► GND

Alimentación motores: Fuente 5V/2A → VMs del DRV8833
                        GND de fuente → GND del ESP32 (común)
```

**Lógica de control:**
- Bomba ON: `digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW)` → sentido forward
- Bomba OFF: `digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW)` → brake
- Ventilador ON: `digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW)` → forward
- Ventilador OFF: `digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW)` → brake

**Nota:** El DRV8833 comparte GPIO4 y GPIO7 con UART1 del DFPlayer. El DRV8833 controla los pines directamente; el DFPlayer solo usa UART, no GPIO digital.

### 3.2 74HC595 (Shift Register para luces LED)

```
ESP32-S3              74HC595              LEDs
─────────             ────────             ────
GPIO8  (DS) ─────────► DS (14)          Q0 ──► Resistencia ──► LED Luz Sala
GPIO11 (SHCP) ────────► SH_CP (11)       Q1 ──► Resistencia ──► LED Luz Cuarto
GPIO18 (STCP) ────────► ST_CP (12)       Q2 ──► Resistencia ──► LED Luz Cultivo
                       VCC (16) ────────► VCC
                       GND (8) ─────────► GND
                       OE  (13) ────────► GND (habilitado siempre)
                       MR  (10) ────────► VCC

Secuencia de 8 bits:
  bit0 = Q0 (Luz Sala)
  bit1 = Q1 (Luz Cuarto)
  bit2 = Q2 (Luz Cultivo)
```

**Secuencia de envío:**
```cpp
digitalWrite(DS, bit_value);   // coloca bit en DS
digitalWrite(SHCP, LOW);       // pulso de desplazamiento
digitalWrite(SHCP, HIGH);
digitalWrite(STCP, LOW);       // bloquea registro de desplazamiento
digitalWrite(STCP, HIGH);      // actualiza salidas
```

**Resistencias:** Cada LED necesita una resistencia limitadora (típicamente 220Ω–470Ω) entre Q0/Q1/Q2 y el LED.

**Pines sacrificados:** GPIO11 (SHCP) y GPIO18 (STCP) reemplazan los botones físicos SILENCIO y MODO. La navegación se reimplementa por software.

### 3.3 DFPlayer Mini (Reproductor MP3)

```
ESP32-S3              DFPlayer Mini
─────────             ─────────────
GPIO4  (UART1 RX) ────► TX   (envía datos desde ESP32 al DFPlayer)
GPIO7  (UART1 TX) ────► RX   (recibe datos del DFPlayer al ESP32)
                      BUSY  ────► GPIO de detección (opcional, -1 en firmware)
                      VCC   ────► 5V (o 3.3V según módulo)
                      GND   ────► GND
                      SPK1/SPK2 ──► Parlante 4Ω/3W
                      RX    ────► Resistencia 1k (pull-up)
                      TX    ────► Resistencia 1k (pull-up)
```

**Nota:** UART1 se remapea a GPIO4/RX y GPIO7/TX (compartidos con DRV8833). El DRV8833 usa estos GPIOs como salidas digitales, y el DFPlayer los usa como UART. El firmware llama a `transporteDFPlayer.begin(4, 7, MP3_BUSY_PIN)`.

**Partición de audio:** Se necesitan 168 archivos MP3 (1 por cada tecla del control remoto IR) en la carpeta `/mp3` de la tarjeta MicroSD del DFPlayer.

### 3.4 DHT11 (Sensor ambiental)

```
ESP32-S3              DHT11
─────────             ────
GPIO14 (data) ────────► DATA
                       VCC ──► 3.3V
                       GND ──► GND
                       (opcional: R=4.7k pull-up entre VCC y DATA)
```

### 3.5 LCD 1602 con módulo I2C (PCF8574)

```
ESP32-S3              LCD I2C (PCF8574)
─────────             ─────────────────
GPIO17 (SDA) ─────────► SDA
GPIO13 (SCL) ─────────► SCL
                        VCC ──► 3.3V o 5V
                        GND ──► GND
```

**Direcciones I2C comunes:** 0x27 (más común), 0x3F

### 3.6 LDR (Fotoresistor)

```
3V3 ──── R=10k ────┬──── GPIO3 (lectura ADC)
                    │
                   LDR
                    │
                   GND
```

**Divisor de voltaje:** El fotoresistor y una resistencia fija de 10kΩ forman un divisor entre 3V3 y GND. El punto medio se conecta a GPIO3. El firmware lee el valor ADC (0–4095) y lo convierte a `LUZ_PCT` (0–100%).

### 3.7 Sensor de humedad de tierra (suelo)

```
ESP32-S3 (ADC1)       Sensor de humedad
─────────────         ─────────────────
GPIO15 (ADC) ────────► Salida analógica del sensor
                       VCC ──► 3.3V
                       GND ──► GND
```

### 3.8 Sensor de nivel de agua

```
ESP32-S3 (ADC1)       Sensor de nivel
─────────────         ──────────────
GPIO16 (ADC) ────────► Salida analógica
                       VCC ──► 3.3V
                       GND ──► GND
```

### 3.9 PIR (Sensor de movimiento)

```
ESP32-S3              PIR (HC-SR501)
─────────             ──────────────
GPIO9  ──────────────► OUT
                       VCC ──► 3.3V o 5V
                       GND ──► GND
```

### 3.10 Botón de paro de emergencia

```
ESP32-S3              Botón PARO
─────────             ────────
GPIO10 (INPUT_PULLUP)──┤├── Botón
                       GND ──► un extremo del botón
```

**Lógica:** El botón conecta GPIO10 a GND. El firmware configura `INPUT_PULLUP`, así que sin pulsar el pin lee HIGH (sin paro). Al pulsar, lee LOW (paro activo).

### 3.11 Receptor IR (IR Remote)

```
ESP32-S3              IR Receiver (VS1838B o similar)
─────────             ────────────────────────────────
GPIO12 ───────────────► OUT
                       VCC ──► 3.3V
                       GND ──► GND
```

---

## 4. Diagrama de conexiones completo (texto)

```
                         ┌─────────────────────────────────────────────────────────┐
                         │                    ESP32-S3 N16R8                      │
                         │                    (COM9 - USB CH343)                 │
                         │                                                             │
                         │  GPIO3  ◄──── LDR (divisor 10k/3V3)                      │
                         │  GPIO4  ──► DRV8833 AIN1 ──► Bomba ──► [5V/2A]          │
                         │  GPIO4  ──► UART1 RX ◄──── DFPlayer TX                  │
                         │  GPIO5  ──► DRV8833 BIN1 ──► Ventilador ──► [5V/2A]     │
                         │  GPIO6  ──► DRV8833 BIN2 ──► Ventilador ──► [5V/2A]     │
                         │  GPIO7  ──► DRV8833 AIN2 ──► Bomba                      │
                         │  GPIO7  ──► UART1 TX ──────► DFPlayer RX                │
                         │  GPIO8  ──► 74HC595 DS ──► Q0 Luz Sala + R(220Ω)        │
                         │  GPIO9  ◄──── PIR OUT                                     │
                         │  GPIO10 ◄──── Botón PARO (INPUT_PULLUP)                  │
                         │  GPIO11 ──► 74HC595 SHCP                                  │
                         │  GPIO12 ◄──── IR Receiver OUT                             │
                         │  GPIO13 ──► LCD SCL (I2C)                                 │
                         │  GPIO14 ──► DHT11 DATA                                    │
                         │  GPIO15 ──► Sensor suelo (ADC1)                          │
                         │  GPIO16 ──► Sensor nivel agua (ADC1)                     │
                         │  GPIO17 ──► LCD SDA (I2C)                                 │
                         │  GPIO18 ──► 74HC595 STCP                                  │
                         │                                                             │
                         │  VCC3V3 ──► Todos los VCC de sensores + I2C             │
                         │  GND  ──► Tierra común                                    │
                         │                                                             │
                         └─────────────────────────────────────────────────────────┘
                                         │
                         ┌───────────────┼───────────────┐
                         ▼               ▼               ▼
                   ┌───────────┐  ┌───────────┐  ┌───────────┐
                   │ DRV8833   │  │ 74HC595   │  │ DFPlayer  │
                   │           │  │           │  │ Mini      │
                   │ AIN1/AIN2 │  │ DS/SHCP/  │  │ UART1     │
                   │ BIN1/BIN2 │  │ STCP      │  │ TX/RX     │
                   │ nSLEEP→VCC│  │ Q0-Q2→LEDs│  │ BUSY      │
                   └───────────┘  └───────────┘  └───────────┘
                         │               │               │
                   ┌─────▼─────┐  ┌─────▼─────┐  ┌─────▼─────┐
                   │ Bomba 5V  │  │ 3 LEDs    │  │ Parlante  │
                   │ + Ventila │  │ 220Ω      │  │ 4Ω/3W     │
                   │ dor       │  │           │  │ + SD Card │
                   └───────────┘  └───────────┘  └───────────┘
```

---

## 5. Tabla resumen de todos los pines

| GPIO | Función primaria | Función secundaria | Tipo | Módulo(s) |
|------|-----------------|-------------------|------|-----------|
| 3 | LDR | — | Entrada analógica | Sensor luz |
| 4 | DRV8833 AIN1 | UART1 RX | Salida digital | Bomba + DFPlayer |
| 5 | DRV8833 BIN1 | Relé sala | Salida digital | Ventilador |
| 6 | DRV8833 BIN2 | Relé cuarto | Salida digital | Ventilador |
| 7 | DRV8833 AIN2 | UART1 TX | Salida digital | Bomba + DFPlayer |
| 8 | 74HC595 DS | Relé invernadero | Salida digital | Luces |
| 9 | PIR | — | Entrada digital | Seguridad |
| 10 | PARO emergencia | — | Entrada digital | Seguridad |
| 11 | 74HC595 SHCP | Botón SILENCIO (sacrificado) | Salida digital | Luces |
| 12 | IR Receiver | — | Entrada digital | Control IR |
| 13 | LCD SCL | — | Salida digital (I2C) | Pantalla |
| 14 | DHT11 DATA | — | Entrada digital | Sensor ambiental |
| 15 | ADC1 suelo | — | Entrada analógica | Sensor humedad |
| 16 | ADC1 nivel | — | Entrada analógica | Sensor agua |
| 17 | LCD SDA | — | Salida digital (I2C) | Pantalla |
| 18 | 74HC595 STCP | Botón MODO (sacrificado) | Salida digital | Luces |

---

## 6. Fuente de alimentación

| Componente | Voltaje | Corriente | Conexión |
|-----------|---------|-----------|----------|
| ESP32-S3 | 3.3V | ~240mA | USB (CH343) |
| DRV8833 (Vms) | 5V | Variable según carga | Fuente 5V/2A |
| 74HC595 | 3.3V | ~20mA | GPIO VCC |
| DFPlayer Mini | 5V | ~100mA | Fuente 5V/2A |
| LCD I2C | 5V | ~100mA | Fuente 5V/2A |
| DHT11 | 3.3V | ~1mA | GPIO VCC |
| LDR | 3.3V | — | Divisor 3V3 |
| PIR | 3.3V–5V | ~20mA | GPIO VCC |
| Motores | 5V | Hasta 2A | Fuente 5V/2A (vía DRV8833) |

**Nota:** El GND del ESP32, la fuente 5V/2A y todos los módulos deben estar conectados a la misma tierra común para evitar problemas de referencia de voltaje.

---

## 7. Perfil de compilación

```cpp
// Definido en casa_inteligente_v4.ino:
#define DOMUS_PERFIL_CASA 4          // Perfil CASA_FINAL_DRV8833_DFPLAYER
#define DOMUS_SALIDAS_ECONOMICAS 1   // Variante montada: LED directo, activas en HIGH
#define MICROSD_HABILITADA false     // MicroSD deshabilitado
#define BOMBA_DIRECTA_S8050 true     // Bomba por GPIO directo (S8050), activa en HIGH
#define IR_CASA_HABILITADO true      // IR habilitado (perfil >= 3)

// Definido en domus_drivers.h:
#define DOMUS_DRIVER 1               // DRV8833 seleccionado
#define DOMUS_DRIVER_VALIDADO 0      // F1 aún no acreditada (cambio manual)
#define PERFIL_CON_DRV8833 true      // Perfil 4
#define AUDIO_CANDIDATO_HABILITADO false // Audio depende de DRIVER_VALIDADO
```

---

## 8. Firmware subido

**Fecha:** 2026-09-18
**Sketch:** `casa_inteligente_v4.ino` (perfil 4)
**Compilación:** Exitosa (422873 bytes flash, 28904 bytes RAM)
**Subida:** Exitosa via `arduino-cli` a COM9 (ESP32-S3)
**Herramienta:** `arduino-cli` v1.5.1, `esptool` v5.3.1
**FQBN:** `esp32:esp32:esp32s3`
**Partición:** `app3M_fat9M_16MB`

---

## Anexo · Visualizadores 3D y Diagramas

| Archivo | Descripción |
|---------|-------------|
| `hardware/planos/modelo_3d_con_componentes.html` | Gemelo digital 3D interactivo con todos los componentes electrónicos implementados (ESP32, DRV8833, 74HC595, DFPlayer, sensores, LEDs, relés). Vistas: isométrica, frontal, planta, interior, sistemas, **componentes**, cableado, explotada. |
| `hardware/planos/diagrama_conexiones.html` | Diagrama de conexiones eléctricas detallado con tabla de pines, wiring diagram por módulo, tabla resumen de 16 conexiones, flujo de control y tabla de alimentación. |
| `hardware/planos/modelo_3d_interactivo.html` | Modelo original v4 sin componentes (solo estructura física y reservas transparentes). |

### Cómo usar el modelo 3D
1. Abre `modelo_3d_con_componentes.html` en un navegador (Chrome, Firefox, Edge).
2. Las vistas están en el panel izquierdo: **Componentes** y **Cableado** son las nuevas.
3. Pasa el ratón sobre el canvas para rotar. Rueda para zoom. Arrastra para mover.
4. Toca una pieza para ver su ficha (nombre, descripción, capa, material).
5. Usa los controles deslizantes de explosión y opacidad para ver el interior.
6. Las teclas 1-5 cambian de vista rápida.
