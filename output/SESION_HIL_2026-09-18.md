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

## Al despertar

1. Revisar cableado de suelo y nivel (lo más probable: VCC o GND sueltos).
2. DHT11: resistencia entre p1–p2 y `DHT_FALLOS` en `DIAGNOSTICO`.
3. Aprender el mando y calibrar (`CAL_*` + `CAL_GUARDAR`).
4. Bomba: diodo 1N4007 + `OUT-`→GND, prueba sumergida.
