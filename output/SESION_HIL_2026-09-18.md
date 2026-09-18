# Sesión HIL nocturna — 2026-09-18 (COM9, firmware v4.3+)

## Resultado: HIL 8/8 en verde (dos rondas)

| Test | Estado |
|---|---|
| 01 identidad y diagnóstico | PASS (perfil 3, `BOMBA_ETAPA=S8050_GPIO4`) |
| 02 estado con sensores y seguridad | PASS |
| 03 prueba guiada completa | PASS |
| 04 comandos inválidos | PASS (`NACK;HOLA;no_reconocido`, `longitud_invalida`) |
| 05 LEDs ida y vuelta | PASS (sala, cuarto, cultivo) |
| 06 IR 21 teclas | PASS (`INDICE=0`…`20`) |
| 07 stream `SENSORES;` | PASS (cada 2 s) |
| 08 PARO bloquea y rearme no enciende | PASS (todo queda en 0) |

## Foto de sensores (placa real)

```
SENSORES;TEMP_C=-1.0;HUM_AIRE=-1.0;HUM_PCT=-1;NIVEL=-1;LUZ_PCT=100/98/86;PIR=1;SALIDAS=00000;
```

| Sensor | Estado | Acción pendiente |
|---|---|---|
| Suelo (GPIO15) | Fuera de rango (-1) | Revisar VCC/GND/AO; calibrar seco/húmedo |
| Nivel (GPIO16) | Fuera de rango (-1) | Revisar S/+/-; calibrar vacío/lleno |
| DHT11 (GPIO14) | NaN, suspendido con reintento 60 s | Revisar cableado + R 5.1–10 kΩ p1–p2 |
| LDR (GPIO3) | **NO VERIFICADO** (ver nota abajo) | Conectarlo y hacer prueba de sombra |
> **Corrección**: el 100→98→86 era un pin flotante a la deriva, no el LDR
> (el dueño confirmó que no estaba conectado). Un ADC sin nada enchufado
> capta ruido y el porcentaje, truncado a 100, parece "vivo". Lección:
> que varíe no prueba nada; la prueba real es taparlo y verlo **caer en
> picada**. Ningún sensor analógico queda confirmado vivo.
| PIR (GPIO9) | HIGH fijo | Probar reposo 60 s + potenciómetros |
| LCD 0x27 / IR | PASS | Aprender 21 teclas (`IR_GRABAR_*`) |
| Salidas / PARO / watchdog | Todo en 0, `REINICIOS_CRITICOS=0` | Sano |

## Hallazgo y fix de la noche: flood de errores

- **Antes**: 86 913 errores acumulados; el Serial spameaba ~60 líneas `[ERROR]`/s
  (suelo + nivel fuera de rango en cada vuelta del `loop()`).
- **Fix subido a la placa**: cada origen de sensor avisa como máximo 1 vez
  cada 30 s (`INTERVALO_AVISO_SENSOR_MS`); `LONGITUD_MAX_ERROR` 40→64
  (el `ULTIMO_ERROR` salía truncado a "…r").
- **Verificado**: 12 s de Serial → 6 líneas (todas `SENSORES;`), **0 errores**.
- Compilación Arduino: 13% flash / 8% RAM. Tests host: 102 passed, 0 failed.

## Diagnóstico PINTEST (comando nuevo: `PINTEST <gpio>`)

Crudo / pull-up / pull-down medidos en la placa real:

| GPIO | Crudo | PullUp | PullDown | Lectura |
|---|---|---|---|---|
| 16 (nivel) | 339 | 2967 | 377 | Cargado a GND: módulo a medio conectar o fila compartida en la protoboard apretada. NO flotante puro |
| 15 (suelo) | 4095 | 4095 | 3688 | Forzado a HIGH: módulo alimentado con sonda seca (o AO a 3V3). Probar sonda en agua |
| 3 (LDR) | 1435 | 1433 | 1434 | **Divisor real, clavado**: LDR confirmado vivo |
| 14 (DHT) | 3312 | 3316 | 2637 | Pull-up externo de 10k presente; el chip no responde → revisar VCC pata 1 |
| 9 (PIR) | 4052 | 4007 | 3987 | Forzado a HIGH de verdad: probar reposo 60 s + pote de tiempo al mínimo |

Lección doble: un pin flotante puede leer "válido" estable (~1480) y un
sensor a medio conectar también. `leerSensorPromediado()` ahora devuelve -1
si el pin sigue los pulls a los rieles (>3500 y <600); comando `PINTEST`
para medir a mano. Suite: 112 passed (HIL 8/8 incluido).

## Al despertar

1. Nivel: con todo desconectado del sensor, `PINTEST 16` debe decir
   FLOTANTE; si no, el jumper del GPIO16 toca otra fila en la protoboard.
2. Suelo: meter la sonda en agua y mirar `SENSORES;` (debe aparecer HUM_PCT).
3. DHT11: verificar 3V3 en pata 1 (el pull-up de 10k sí está).
4. PIR: cuarto solo 60 s, pote de tiempo al mínimo.
5. Aprender el mando y calibrar (`CAL_*` + `CAL_GUARDAR`).
6. Bomba: diodo 1N4007 + `OUT-`→GND, prueba sumergida.
