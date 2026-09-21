---
estado: vigente
fecha: 2026-09-21
autoridad: 78
tipo: guia-conexiones-v5
depends: 74, 75, 76, 77
---

# 78 - Guia de Conexiones Completa v5

> [!IMPORTANT]
> Guia paso a paso para conectar todo el hardware. Sin experiencia previa requerida.
> Archivo visual: `hardware/Conexion_Domus_Guia_Completa.html`

## Distribucion: 2 Protoboards

```
PROTOBOARD 1 (LOGICA)          PROTOBOARD 2 (CARGAS)
Alimentada por USB ESP32        Alimentada por 5V/2A
─────────────────────           ─────────────────────
• ESP32-S3 N16R8                • Bomba DC + S8050
• LCD1602 I2C                   • 6 LEDs (casa/porche/cultivo)
• VS1838B receptor IR           • DFPlayer Mini + parlante
• DHT11                         • Sensor suelo
• LDR                           • Sensor nivel agua
• LED VIVO (breathing)          
• Boton PARO                    

         ══════════════════════════════
         ║  GND COMUN (cable grueso)  ║
         ══════════════════════════════
         ║  8 cables GPIO (señales)  ║
         ══════════════════════════════
```

## Mapa GPIO y colores de cable

| GPIO | Funcion | Color | Protoboard |
|------|---------|-------|------------|
| 3 | LDR | Morado | 1 |
| 4 | Bomba | Rojo | 2 |
| 5 | Casa (2 LEDs) | Amarillo | 2 |
| 7 | Cultivo (3 LEDs) | Azul | 2 |
| 8 | Porche (1 LED) | Verde | 2 |
| 9 | Jarvis VIVO | Blanco | 1 |
| 10 | PARO | Gris | 1 |
| 11 | DFPlayer RX | Naranja | 2 |
| 12 | IR VS1838B | Amarillo osc. | 1 |
| 13 | LCD SCL | Azul claro | 1 |
| 14 | DHT11 | Naranja | 1 |
| 15 | Sensor suelo | Marron | 2 |
| 16 | Sensor nivel | Gris osc. | 2 |
| 17 | LCD SDA | Verde | 1 |
| 18 | DFPlayer TX | Morado | 2 |

## Conexiones detalladas

### LCD1602 I2C (Protoboard 1)
```
LCD VCC → 5V ESP32
LCD GND → GND ESP32
LCD SDA → GPIO17
LCD SCL → GPIO13
```

### VS1838B IR (Protoboard 1)
```
OUT → GPIO12
GND → GND
VCC → 3V3
```

### DHT11 (Protoboard 1)
```
VCC (pin1) → 3V3
DATA (pin2) → GPIO14 + resistor 10k a 3V3
GND (pin4) → GND
```
IMPORTANTE: Pull-up 10k es OBLIGATORIO.

### LDR (Protoboard 1)
```
3V3 → LDR → GPIO3 → 10k → GND
```

### LED VIVO (Protoboard 1)
```
GPIO9 → 1k → Base S8050
Colector → LED → 330ohm → 5V
Emisor → GND
```

### Boton PARO (Protoboard 1)
```
GPIO10 → boton → GND
(INPUT_PULLUP, sin resistor externo)
```

### Bomba (Protoboard 2)
```
GPIO4(Proto1) → 1k → Base S8050
Emisor S8050 → GND
Colector S8050 → Bomba(-)
Bomba(+) → 5V/2A
1N4007: cátodo→Bomba(+), ánodo→Bomba(-)
```

### LEDs Casa (Protoboard 2)
```
GPIO5(Proto1) → 330ohm → Anodo LED1 + Anodo LED2
Cathodo LED1 + Cathodo LED2 → GND
```

### LED Porche (Protoboard 2)
```
GPIO8(Proto1) → 330ohm → Anodo LED
Cathodo → GND
```

### LEDs Cultivo (Protoboard 2)
```
GPIO7(Proto1) → 100ohm → Anodo LED1 + Anodo LED2 + Anodo LED3
Cathodos → GND
```

### DFPlayer Mini (Protoboard 2)
```
VCC → 5V/2A
GND → GND
RX → GPIO18(Proto1) TX  ← CRUZADO
TX → GPIO11(Proto1) RX  ← CRUZADO
SPK_1/SPK_2 → Parlante 4ohm
```

### Sensor Suelo (Protoboard 2)
```
VCC → 5V/2A
GND → GND
AO → GPIO15(Proto1)
```

### Sensor Nivel (Protoboard 2)
```
VCC → 5V/2A
GND → GND
S → GPIO16(Proto1)
```

## Alimentacion dual

```
USB-C → ESP32 (logica, LCD, sensores)
5V/2A → Bomba, DFPlayer, LEDs (cargas)
GND comun entre ambas fuentes (OBLIGATORIO)
```

Consumo: bomba 0.5-0.6A + DFPlayer 0.2A + LEDs 0.1A + ESP32 0.15A = ~1.05A max.
Fuente 2A sobra.

## Checklist pre-encendido

- [ ] GND comun entre protoboards
- [ ] Alimentacion dual funcionando
- [ ] Resistencias: 330ohm (LEDs), 100ohm (cultivo), 1k (S8050), 10k (DHT11, LDR)
- [ ] Diodo 1N4007 en bomba (cátodo a +)
- [ ] DFPlayer RX/TX cruzados
- [ ] MicroSD FAT32 con carpetas 01-21 y 51-71
- [ ] S8050 orientacion: CBE (Collector, Base, Emitter)
- [ ] LEDs polaridad: anodo mas largo (+), cathodo mas corto (-)

## Relacionadas

- [[74 - Hardware final y mapa GPIO v5]]
- [[75 - Audio Jarvis remodelado]]
- [[76 - Control IR y modo inteligente]]
- [[77 - Plan firmware final v5]]
