# Diagrama de conexiones — Banco actual (`BANCO_COMPLETO_S8050_IR`)

> Versión web interactiva (diagramas Hoy/Futuro): `visualizaciones/banco_hoy_futuro.html` — ábrela en el navegador.

Perfil 3 del firmware `casa_inteligente_v4.ino` (única fuente: `MAPA_CASA`).
Placa: **ESP32-S3**. Si un cable no coincide con esta tabla, manda la tabla:
el firmware no admite pines fuera de ella.

## Reglas de oro (lee esto primero)

1. **Un solo GND común.** El GND del ESP32, el `OUT-` del TP4056, el emisor del S8050, el GND del LCD, del IR, del PIR, del DHT y la pata de abajo del divisor LDR van **todos unidos**. El 90% de "no funciona" es un GND suelto.
2. **Botones a GND, nunca a 5V ni 3V3.** El firmware activa pull-up interno: botón sin pulsar = HIGH, pulsado = LOW (conecta el pin a GND).
3. **3V3 para señales, 5V solo para potencia.** Sensores y LCD a 3V3. La bomba come del TP4056 (`OUT+`), jamás de un GPIO ni del pin 3V3.
4. **El diodo 1N4007 es obligatorio** antes de energizar la bomba (ver sección bomba).
5. **LCD y DHT se conectan con la placa APAGADA.** El LCD se escanea al arrancar.

## Tabla completa de pines

| GPIO | Función | Conexión exacta |
|---|---|---|
| 3 | LDR (luz) | Punto medio del divisor: 3V3 → LDR → GPIO3 → R 10k → GND |
| 4 | Bomba (S8050) | R 1 kΩ → base del S8050 (activa en HIGH) |
| 5 | Luz sala (LED + R) | GPIO → R 220 Ω → ánodo LED (pata larga), cátodo (corta) → GND |
| 6 | Luz cuarto (LED + R) | Igual que sala |
| 7 | Ventilador | **BLOQUEADO**: configurado como entrada, no conectar nada |
| 8 | Luz cultivo (LED + R) | Igual que sala |
| 9 | PIR (presencia) | OUT → GPIO9; VCC → 5V (pin VU); GND → GND común |
| 10 | Botón PARO | GPIO → botón → GND |
| 11 | Botón SILENCIO | GPIO → botón → GND |
| 12 | IR HX1838 | Señal (OUT) → GPIO12; VCC → 3V3; GND → GND común (mirando el frente del sensor: OUT-VCC-GND según tu módulo, verifica etiqueta) |
| 13 | LCD SCL | Directo al pin SCL del backpack I2C |
| 14 | DHT11 DATA | Pata 2 del DHT; pata 1 → 3V3, pata 4 → GND, R 5.1–10 kΩ entre patas 1 y 2 |
| 15 | Suelo (analógico) | Salida AO del sensor → GPIO15 (VCC → 3V3, GND → GND) |
| 16 | Nivel agua (analógico) | Salida S del sensor → GPIO16 (VCC → 3V3, GND → GND) |
| 17 | LCD SDA | Directo al pin SDA del backpack I2C |
| 18 | Botón MODO (página LCD) | GPIO → botón → GND |
| 3V3 | Alimentación señales | LCD VCC, IR VCC, DHT pata 1, VCC sensores suelo/nivel, extremo del LDR |
| GND | Tierra común | Ver regla 1 |
| 5V (VU) | Potencia | TP4056 (IN+/-) y PIR VCC |

## Diagrama general (ASCII)

```text
                    +-------------------+
                    |     ESP32-S3      |
                    |                   |
  3V3 --+-----------+-> 3V3             |
        |           |                   |
        +--> LCD VCC |                   |
        +--> IR VCC  |                   |
        +--> DHT p1  |                   |
        +--> SUELO VCC                  |
        +--> NIVEL VCC                  |
        +--> LDR (extremo)              |
                    |                   |
  GND --+-----------> GND  (COMÚN, ver regla 1)
        |           |                   |
        +--> LCD GND |   GPIO3  <--- punto medio LDR (3V3-LDR-+-R10k-GND)
        +--> IR GND  |   GPIO4  ---> R 1k ---> base S8050
        +--> DHT p4  |   GPIO5  ---> R220 ---> LED sala ---> GND
        +--> SUELO GND              GPIO6  ---> R220 ---> LED cuarto --> GND
        +--> NIVEL GND  |   GPIO7  = BLOQUEADO (nada)
        +--> OUT- TP4056|   GPIO8  ---> R220 ---> LED cultivo -> GND
        +--> emisor S8050            GPIO9  <--- OUT del PIR
        +--> botones (3x)           GPIO10 ---> [PARO] ---> GND
        +--> cátodos LED |   GPIO11 ---> [SILENCIO] -> GND
        |           |   GPIO12 <--- OUT del HX1838
  LCD SDA <---------+--- GPIO17         GPIO13 ---> SCL del LCD
  LCD SCL <---------+--- GPIO13         GPIO14 <---> DATA DHT11 (p2)
  SUELO AO --------->--- GPIO15         GPIO18 ---> [MODO] ---> GND
  NIVEL S ---------->--- GPIO16
                    |                   |
  5V (VU) ---> TP4056 IN+ / PIR VCC     |
  USB ---> alimenta ESP32               |
                    +-------------------+
```

## La bomba con S8050 (lee con calma, es la parte delicada)

```text
  TP4056 OUT+ ----+----> (+) BOMBA (-) ----> colector S8050
                  |                              |
               1N4007                         base <-- R 1k <-- GPIO4
           (RAYA hacia OUT+)                     |
                  |                           R 10k
  TP4056 OUT- ----+----> GND COMÚN <------ emisor S8050
                         (unido al GND del ESP32)
```

Paso a paso:
1. **Identifica las patas del S8050** mirando su cara plana con las patas hacia abajo: de izquierda a derecha son **emisor, base, colector** (verifica con tu pieza: algunos lotes cambian; si dudas, mide con multímetro en modo diodo).
2. **Emisor → GND común.** Directo, cable corto y grueso.
3. **Base → R 1 kΩ → GPIO4**, y de la base también **R 10 kΩ → GND** (pull-down: mantiene la bomba apagada mientras el ESP32 arranca).
4. **Colector → cable negativo de la bomba.**
5. **Positivo de la bomba → `OUT+` del TP4056.**
6. **`OUT-` del TP4056 → GND común del ESP32.** Sin este cable la bomba no tiene retorno y no gira (causa nº 1 de "no se mueve").
7. **Diodo 1N4007 en paralelo con la bomba, raya hacia `OUT+`.** Lo más pegado posible a los cables del motor.

### Por qué tu bomba no se movió (lista de sospechosos en orden)
1. Falta el cable `OUT-` → GND del ESP32 (retorno inexistente).
2. Patas del S8050 mal identificadas (emisor/colector swapped = no conduce).
3. TP4056 sin batería/fuente en `IN` (el TP4056 necesita alimentación de entrada; `OUT` sin `IN` no da nada).
4. Bomba probada fuera del agua o trabada (las minibombas se prueban **sumergidas**).
5. GPIO4 en LOW permanente: verifica con `ESTADO` que el firmware reporta la salida (recuerda: arranca bloqueada en `MANUAL_OFF`; hay que mandar `RIEGO_ON`).

## Dudas frecuentes

**¿El LED lleva resistencia? ¿De qué valor?** Sí, siempre: 220 Ω típico entre GPIO y ánodo. Sin ella quemas el LED y estresas el GPIO.

**¿Pata larga o corta del LED?** Larga (ánodo) hacia la resistencia/GPIO; corta (cátodo) a GND. Si lo pones al revés simplemente no enciende (no se quema con la resistencia puesta).

**¿DHT11 qué patas?** Mirando la rejilla de frente, patas abajo, de izquierda a derecha: 1 = VCC (3V3), 2 = DATA (GPIO14), 3 = libre, 4 = GND. Resistencia 5.1–10 kΩ **entre patas 1 y 2** (no en serie con DATA). Si da NaN siempre: 90% es esa resistencia o la pata 3 tocando algo.

**¿LCD no muestra nada?** Dirección `0x27` (el firmware la detecta solo). Contraste: gira el potenciómetro azul del backpack. SDA→17, SCL→13, y **conectar antes de encender**. A 50 kHz es lento a propósito para evitar parpadeos.

**¿IR no responde?** El mando debe estar **aprendido**: `IR_GRABAR_0` … `IR_GRABAR_20` pulsando cada tecla. Comprueba con `IR_LISTA` (`APRENDIDAS=n/21`). Sin aprender, las teclas se rechazan por diseño.

**¿PIR siempre en HIGH?** Tiene dos potenciómetros naranjas: sensibilidad y tiempo. Empieza con sensibilidad a la mitad y tiempo al mínimo. Debe quedar quieto 30–60 s al energizar (se estabiliza).

**¿Puedo conectar el parlante/DFPlayer ya?** No en este banco: RX/TX/BUSY están en -1 y el audio deshabilitado. Espera al perfil `CASA_FINAL` con GPIO auditados.

**¿Qué NO debe pasar nunca?**
- 5V a un GPIO, a un botón o al DHT → daño permanente.
- Bomba alimentada desde 3V3 o desde un GPIO.
- Bocinas de 1–2 Ω al DFPlayer/GPIO/3V3.
- Desconectar el LCD/SDA/SCL con la placa encendida.

## Checklist antes de energizar (léelo en voz alta con la placa apagada)

- [ ] Todos los GND unidos en un punto común.
- [ ] Ningún cable a GPIO7.
- [ ] Diodo 1N4007 puesto, raya a `OUT+`.
- [ ] `OUT-` unido al GND del ESP32.
- [ ] Botones van a GND, no a voltaje.
- [ ] LEDs con resistencia y polaridad correcta.
- [ ] LCD y DHT conectados antes de encender.
- [ ] Bomba sumergida antes de `RIEGO_ON`.
