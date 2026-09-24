# DOMUS — firmware único y perfil de producto

`casa_inteligente_v4.ino` es la única base funcional. El perfil predeterminado
actual es `DOMUS_PERFIL_CASA=4` (`CASA_FINAL_DRV8833_DFPLAYER`): DRV8833,
DFPlayer y HIL del producto.

## Hardware real (inventario autorizado)

- Bomba: GPIO4 → DRV8833 canal A (AIN1/AIN2 = GPIO4/GPIO7); solo canal A.
- Casa / Porche / Spare(Jarvis): LED en GPIO5 / GPIO8 / GPIO6 (2 azules).
- Cultivo NO tiene luz: índice 3 siempre `SALIDA_FISICA=false`; sin comandos.
- Sin sensor de nivel de agua: la bomba solo se gobierna por humedad de
  suelo y `TIEMPO_MAXIMO_BOMBA_MS` (timeout). `MAPA_CASA.nivel` (GPIO16)
  queda reservado por unicidad de pines, nunca se lee.
- IR HX1838: señal en GPIO12. DHT11 GPIO14, suelo GPIO15, LDR GPIO3.
- DFPlayer: RX GPIO17 / TX GPIO18 (LCD descartado, quemado).
- Botones a GND con pull-up: PARO GPIO10, SILENCIO GPIO9, MODO GPIO18.
- GPIO11 no existe en la placa; SILENCIO usa GPIO9.

## Aprender el mando IR

Con el LCD conectado antes de encender o reiniciar, cada pulsación válida se
muestra durante 5 segundos. La primera línea contiene protocolo y dirección
(`IR P7 A00FF`) y la segunda el comando que debes anotar (`CMD 0x0019`). Los
fallos normales de sensores no tapan esta lectura; PARO y modo seguro sí tienen
prioridad. El mismo dato se emite por Serial como `IR;PROTO=...;ADDR=...;CMD=...`.

Monitor Serial a 115200. Para cada índice de 0 a 20:

```text
IR_GRABAR_0
```

Pulsa la tecla física indicada y continúa hasta `IR_GRABAR_20`. Consulta el
mapa con `IR_LISTA`, el último código con `IR_LEER` y restaura el mapa inicial
con `IR_BORRAR`. Hasta aprender una posición, su código no puede ejecutar una
acción. El firmware rechaza asignar el mismo código a dos teclas y `IR_LISTA`
muestra `APRENDIDAS=n/21`.

Mapa por botón (un toque, sin combinaciones): arriba configuración, abajo
acciones en orden. CH- fija modo manual, CH+ modo automático, CH alterna
Spare (salida 4); PLAY silencia, VOL ajusta volumen, EQ diagnostica, 0 apaga todo,
100+ alterna la voz Carlos/Karla, 200+ rearma. Abajo: 1/Anterior sala,
2/Siguiente cuarto, 3 sin etapa (NACK + Jarvis "No funciono"), 4 todas las
luces (casa+porche+spare), 5 alterna riego, 6/7/8/9 consultas (temp, humedad,
suelo, estado).
Mientras Jarvis habla o 1.5 s tras cada orden, el mando responde
`NACK;IR;OCUPADO` (la tecla 0 y el aprendizaje no se bloquean).

Serial (nota 80): `SPARE_ON/OFF/AUTO`, `TODO_ON` (luces+Spare, sin bomba),
`TODO_OFF`, `DEMO_ON/OFF`. Sin aliases `LUZC_*`/`INVER_*`. El LCD1602 se
descartó (quemado); las llamadas a pantalla son no-op con puntero nulo.

La vista 0 del LCD muestra temperatura y humedad del aire. La vista 1 muestra
solo humedad de suelo (sin sonda de depósito: la segunda línea fija
`Sin deposito`). Toda orden IR/manual fallida hace hablar a Jarvis con la
carpeta FALLO (`No funciono.`, carpeta 22/72).

## Primera prueba completa

La bomba arranca apagada y bloqueada en `MANUAL_OFF`; ninguna lectura
provisional la enciende al conectar la alimentación. Abre Serial a 115200,
espera unos segundos y envía `PRUEBA`. Copia desde `PRUEBA;INICIO` hasta
`PRUEBA;FIN`.

### Ejecutor automático seguro (Windows)

El HIL vigente prueba este firmware, no el esqueleto legado. No flashea y no
enciende bomba ni ventilador; sólo consulta el banco, conmuta los tres LED y
comprueba PARO/rearme. Desde la raíz del repositorio:

```powershell
python -m pip install -r firmware/tests/requirements-hil.txt
arduino-cli board list
$env:DOMUS_PORT = "COM3" # sustituir por el puerto que muestre tu ESP32
python -m unittest firmware.tests.test_hil_producto -v
```

Si aparece más de un puerto, desconecta la placa, ejecuta `arduino-cli board
list`, vuelve a conectarla y repite: el puerto nuevo es el que debes escribir.
Un `SKIP` significa que falta puerto, placa, dependencia o perfil 3 cargado; no
significa PASS. Al terminar, el ejecutor ordena apagar las cinco salidas.

Para probar la bomba manualmente, colócala primero dentro del agua y envía
`RIEGO_ON`; apágala con `RIEGO_OFF`. El timeout de 120 s la corta sola. Envía
`RIEGO_AUTO` para humedad de suelo con histéresis (sin enclavamiento de nivel).

## Bocinas disponibles

No conectar bocinas de 1–2 ohmios directamente a ningún GPIO, 3V3 ni al S8050
de la bomba. El audio permanece deshabilitado hasta disponer de un amplificador
compatible y confirmar la impedancia admitida por este.

El MAX98306 de la compra es un amplificador con entrada analógica, no I2S y no
reproduce archivos por sí solo. La fuente definida es DFPlayer Mini con microSD
y cuatro pistas por evento; sigue deshabilitada hasta asignar UART libre y
validar alimentación, tarjeta y parlante. Ver la nota Obsidian 65.

Las 176 pistas generadas (88 por voz: 22 eventos x 4 variantes) y su
manifiesto reproducible están en `audio/jarvis_sd/`. La voz 1 (Carlos) usa
carpetas 01-22 y la voz 2 (Karla) usa 51-72. Cada botón del mando tiene su
carpeta; la 22 es FALLO (`No funciono.`). Las variantes llevan significado
(1 = ON manual, 2 = OFF manual, 3 = ON automático, 4 = OFF automático). El
botón CH habla con la carpeta 02 (frases de Spare). Ver
`tools/generate_jarvis_audio.py`.
El firmware ya contiene `JarvisAudio` y `DFPlayerTransport`, pero mantiene
RX/TX/BUSY en `-1` y `MP3_HABILITADO=false`: la nota 66 exige identificar los
GPIO libres y el módulo físico antes de crear el perfil final.
