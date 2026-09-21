---
estado: vigente
fecha: 2026-09-20
autoridad: opencode/mimo-v2.5
tipo: auditoria-profunda
alcance: total (firmware, tests, Obsidian, CI/CD, hardware, herramientas)
---

# 73 - Auditoria Profunda Completa del Proyecto

Auditoria exhaustiva de TODO el proyecto PROJECT DOMUS v4. Cubre firmware
(2,729 lineas), 8 headers, 15 test files (2,053 lineas), 74 notas Obsidian,
herramientas Python, CI/CD, hardware, y documentacion. Ejecutada el 2026-09-20
con analisis automatico de codigo y revision cruzada.

---

## 1. CALIFICACION GENERAL POR CATEGORIA

| Categoria | Nota | Justificacion |
|---|---|---|
| Arquitectura firmware | 9.5/10 | static_assert, dispatch centralizado, ownership model, defense-in-depth |
| Headers modulares | 9.0/10 | Excelente separacion, pero drivers.h tiene stub y codigo duplicado |
| Sistema de seguridad | 9.5/10 | 13 barreras, watchdog, modo seguro, rate limiting, GPIO read-back |
| Sistema de errores | 9.0/10 | Buffer circular, rate limiting, diagnostico completo |
| Automatizaciones | 7.0/10 | 5 automaticas + 2 relativas, pero logica duplicada y conflicto en salidas |
| Protocolo Serial | 9.0/10 | ACK/NACK real, 26 comandos, rate limiting, validacion |
| Pantalla LCD | 9.5/10 | Shadow memory, 7 vistas, CGRAM icons, reconexion automatica |
| IR remoto | 9.0/10 | 21 teclas, NVS persistencia, rechazo duplicados |
| Audio Jarvis | 8.0/10 | 168 tracks, 2 voces, pero hardware no validado |
| Tests | 8.0/10 | Defense-in-depth, 5 patrones, pero faltan tests de automatizacion |
| CI/CD | 8.0/10 | 8 configuraciones, matrix profiles, pero YAML roto y tests fragiles |
| Herramientas | 8.5/10 | 12 scripts, validacion automatizada, pero codigo muerto y paths fragiles |
| Documentacion Obsidian | 7.5/10 | 74 notas, jerarquia clara, pero 10+ notas desactualizadas |
| Hardware docs | 9.5/10 | 111 piezas, validacion geometrica, pipeline de generacion |
| **PROMEDIO GENERAL** | **8.5/10** | |

---

## 2. ERRORES CRITICOS EN FIRMWARE (P0)

### 2.1 float usado como bool en automatizaciones combinadas

**Archivo:** casa_inteligente_v4.ino:2703-2704

`
float tempAutoC = 0; float tempAutoValida = false;
leerAmbiente(tempAutoC, tempAutoValida);
`

**Problema:** leerAmbiente(float&, float&) retorna bool (exito/fallo) y
llena dos floats (temperatura, humedad). Aqui el return value se **descarta**.
tempAutoValida recibe la **humedad** (float), que luego se castea a bool
implicitamente. Si humedad = 0.0 -> false -> automatizaciones combinadas
deshabilitadas. Si humedad > 0 -> true. El valor real de validez se pierde.

**Fix:**
`
float tempAutoC = 0, humAutoAire = 0;
bool tempAutoValida = leerAmbiente(tempAutoC, humAutoAire);
`

**Impacto:** Las automatizaciones combinadas (riego+calor, ventilador+PIR) pueden
activarse con datos invalidos o desactivarse con datos validos.

### 2.2 Comando REPETIR inalcanzable

**Archivo:** casa_inteligente_v4.ino:2332

El handler para REPETIR existe pero no esta en COMANDOS_VALIDOS[] (linea 1765).
El flujo lo rechaza con NACK;REPETIR;no_reconocido antes de llegar al handler.
**Codigo muerto total.**

### 2.3 Dos funciones controlan las mismas salidas en el mismo loop

**Archivo:** casa_inteligente_v4.ino:2709-2710

`
revisarLucesAutomaticas();   // controla salidas 1 (Luz Sala) y 4 (Luz Inv.)
revisarLucesCombinadas();    // controla salidas 1 y 4 con logica diferente
`

Ambas se ejecutan en cada loop(), ambas:
- Leen el PIR y escriben ultimaPresenciaValida / ultimaPresenciaMs
- Leen el LDR
- Pueden emitir ordenes contradictorias a las mismas salidas

Resultado: las luces pueden encender y apagarse en el mismo ciclo. El valor
final de ultimaPresenciaValida depende del orden de ejecucion (no-deterministico
si el PIR tiene ruido).

Lo mismo con ventilador: verificarVentiladorAutomatico (solo temp) y
verificarVentiladorCombinado (temp + PIR) controlan la misma salida (indice 3).

### 2.4 registrarEstadistica ignora el parametro valor

**Archivo:** casa_inteligente_v4.ino:741-750

`
void registrarEstadistica(const String &clave, uint32_t valor) {
  if (clave == "TOTAL_ENCENDIDOS") incrementarTotalEncendidos(); // +1 siempre
  // valor NUNCA se usa para asignar
}
`

Si alguien llama registrarEstadistica("TOTAL_ENCENDIDOS", 50), incrementa
en 1, no en 50. El parametro valor es inutil.

### 2.5 Bypass de ejecutarOrdenActuador en automatizaciones relativas

**Archivo:** casa_inteligente_v4.ino:2623, 2637

verificarAutomacionesRelativas llama desactivarSalida() directamente, saltando:
- Ownership update (propietarioSalidas no se actualiza)
- Log en historial
- Audio Jarvis
- Contador de apagados

Cada otra automatizacion pasa por ejecutarOrdenActuador. Esto crea un estado
inconsistente donde un apagado por timeout no se registra correctamente.
---

## 3. ERRORES DE LOGICA Y RACE CONDITIONS (P1)

### 3.1 PIR escrito por dos funciones por ciclo

verificarLucesAutomaticas() y verificarLucesCombinadas() ambas escriben a
ultimaPresenciaValida y ultimaPresenciaMs. Si las dos lecturas del PIR difieren
(ruido electrico), el resultado es no-deterministico y depende del orden de
llamada en el loop.

### 3.2 driverMotoresAplicar() stub siempre retorna false

**Archivo:** domus_drivers.h:77-81

`
bool driverMotoresAplicar(uint8_t canal, bool activar) {
  (void)canal; (void)activar;
  return false;  // <-- STUB: perfil 4 sin implementacion real
}
`

Si DOMUS_DRIVER_VALIDADO=1, el firmware cree que puede controlar motores pero
esta funcion stub siempre falla. Solo funciona el camino directo por GPIO
(linea 589 en .ino).

### 3.3 Test PIR en selftest siempre pasa

**Archivo:** domus_selftest.ino:593

`
if (v==HIGH || v==LOW)  // Siempre true
  DOMUS_PASS;
`

DOMUS_FAIL es inalcanzable. El test del PIR nunca puede fallar.

### 3.4 Buffer mismatch en selftest

**Archivo:** domus_selftest.ino:1082

bufCmd.reserve(48) pero el loop lee hasta 96 caracteres. Hasta 48 chars se
descartan silenciosamente sin aviso al usuario.

### 3.5 Typo en run_semireal_campaign.py

**Archivo:** tools/run_semireal_campaign.py:29

`
class AssertionError(Exception): pass  # Falta la segunda 'i'
`

Cuando require() falla, Python lanza NameError en vez de la excepcion custom.

### 3.6 Heap allocation en error handler

**Archivo:** casa_inteligente_v4.ino:498-500

`
String completo = origen + ": " + mensaje;  // malloc en handler de bajo-memory
`

Si el heap esta bajo (el escenario que genera errores), esto puede colapsar
el error handler. Clasico "error handler que himself genera error".

---

## 4. TYPOS Y ERRORES DE ESCRITURA (P2)

| # | Linea | Error | Correcto |
|---|-------|-------|----------|
| 1 | 548 | "ADC_SUELRO" | "ADC_SUELO" |
| 2 | 672, 685, 722, 744 | totalRiiegosAutomaticos | totalRiegosAutomaticos (doble i) |
| 3 | 1770 | "CAL_SUELDO" | "CAL_SUELO" (pero tampoco existe, solo CAL_SECO/CAL_HUMEDO) |
| 4 | domus_drivers.h:4-10 | Comentarios duplicados | Lineas 4-10 identicas a 15-20 |
| 5 | 1636 | INTERVALO_RIEGO_MS usado para ventilador | Deberia ser INTERVALO_VENTILADOR_MS |
| 6 | 2541 | Magic number 18 para volumen | Deberia ser constante nombrada |
| 7 | 2548 | Log "GPIO UART/BUSY no asignados" | Mensaje enganoso: falla porque MP3_HABILITADO=false |

---

## 5. CODIGO MUERTO Y ALCANZABLE (P2)

| # | Ubicacion | Descripcion |
|---|-----------|-------------|
| 1 | domus_types.h:18 | ORIGEN_WIFI enum -- nunca se usa en ninguna parte |
| 2 | casa_inteligente_v4.ino:640 | sdRegistrosHistorial -- nunca se lee ni escribe |
| 3 | casa_inteligente_v4.ino:1546-1548 | else if vacio con solo comentario |
| 4 | casa_inteligente_v4.ino:1607 | !dia siempre true en contexto (redundante) |
| 5 | generate_design.py:430 | write_svg() -- nunca se llama, write_svg_v3() lo sobreescribe |
| 6 | generate_design.py:640 | write_viewer() -- nunca se llama, write_viewer_v4() lo sobreescribe |
| 7 | test_firmware_contract.py:18 | assertNotRegex para PINES_SALIDAS[8] -- simbolo ya no existe |

---

## 6. POTENCIAL OVERFLOW Y SEGURIDAD (P1)

### 6.1 32-bit cumulative timer overflow

**Archivo:** casa_inteligente_v4.ino:693

estadisticas.tiempoEncendidoBomba += ms usa unsigned long (32 bits en ESP32).
A 120s por ciclo de bomba, overflow despues de ~49 dias de uso acumulado.
El valor se envuelve a 0 silenciosamente. Bajo riesgo para feria de ciencias,
peroShould use 64-bit o saturar.

### 6.2 XSS en generate_design.py

**Archivo:** tools/generate_design.py:671

best.name y best.note se inyectan directamente en innerHTML sin escaping.
Un nombre de pieza con <script>alert(1)</script> ejecutaria JavaScript.
Latent vulnerability si las piezas se cargan de fuentes externas.

### 6.3 PDF output escapa del project root

**Archivo:** tools/generate_design.py:758

ROOT.parents[5] sube 5 niveles desde tools/, resolviendo al home directory
del usuario en vez del directorio del proyecto.

### 6.4 Sin error handling en generate_jarvis_audio.py

**Archivo:** tools/generate_jarvis_audio.py:84

edge_tts.Communicate().save() hace peticion de red. Si la red falla, la
excepcion no se maneja y crashea toda la generacion sin recuperacion parcial.---

## 7. ERRORES EN TESTS (P1)

| # | Archivo | Linea | Error |
|---|---------|-------|-------|
| 1 | test_esqueleto_unico_y_diagnostico.py | 10 | Ruta incorrecta: falta legacy/ en path |
| 2 | test_pantalla_final.py | 339 | result.stdout en vez de result.stderr |
| 3 | test_jarvis_audio.py | 89 | is_file() falla si los MP3 no existen |
| 4 | test_domus_esqueleto_contract.py | 185 | Conteo fragil de Serial.flush() |
| 5 | test_native_firmware.py | 101 | Branch low=false nunca se ejecuta en perfil 0 |
| 6 | test_firmware_contract.py | 74 | False positive en split YAML |
| 7 | test_semireal_campaign.py | 15 | Assert fragil sobre RNG con seed fijo |

---

## 8. ERRORES EN CI/CD (P1)

### 8.1 YAML indentation roto

**Archivo:** firmware-ci.yml:82-85

El matrix entry tiene name: en columna 0 en vez de indentado bajo include: con -.
Esto produce YAML invalido que puede fallar al parsear o silenciosamente dropear
el matrix entry.

### 8.2 Ruta relativa fragil

**Archivo:** firmware-ci.yml:165

compilar-perfiles-alfa compila desde firmware/legacy/domus_esqueleto/ con
include "../casa_inteligente_v4/casa_inteligente_v4.ino". Desde legacy/,
.. resuelve a firmware/legacy/ (no firmware/).

### 8.3 Falta libreria SH110X

**Archivo:** firmware-ci.yml:224

compilar-base-modular instala Adafruit SSD1306 y ST7789 pero no Adafruit SH110X.
Sin la libreria, DOMUS_HAS_SH110X siempre es 0 y la ruta del OLED SH1106 nunca
se compila ni testea en CI.

---

## 9. ERRORES EN HERRAMIENTAS (P2)

| # | Archivo | Linea | Error |
|---|---------|-------|-------|
| 1 | validate_project.py | 34 | Directorios de test faltantes = PASS silencioso |
| 2 | generate_design.py | 758 | PDF escapa del project root |
| 3 | generate_design.py | 671 | XSS potencial en innerHTML |
| 4 | generate_jarvis_audio.py | 84 | Excepcion de red no manejada |
| 5 | generate_jarvis_audio.py | 59 | SHA256 se calcula cada run innecesariamente |

---

## 10. NOTAS OBSIDIAN DESACTUALIZADAS (P2)

### 10.1 Numeros incorrectos

| Nota | Error | Realidad |
|------|-------|----------|
| 67 | 14 eventos, 56 MP3 por voz | 21 eventos, 168 total |
| 66 | 56 frases Jarvis | 168 MP3 |
| 66 | 5 vistas LCD | 7 vistas |
| 67 | Tecla 6 = cambio de voz | Tecla 100+ |
| 50 | Buzzer en GPIO12 | GPIO12 = IR receiver |
| 69 | GPIO 47/48 prohibidos | Codigo los usa para SD |

### 10.2 Info obsoleta sin marcar

| Nota | Contenido obsoleto |
|------|-------------------|
| 50 | Perfiles ALFA reemplazados por PerfilCasa v4 |
| 71 | 74HC595 en perfil 4 (Isaac no tiene) |
| 40 | INMP441+MAX98357A (cambiado a DFPlayer) |
| 66 | GPIO4/7 insuficientes (ya resuelto) |

### 10.3 Contradicciones entre notas

| Nota A | Nota B | Contradiccion |
|--------|--------|---------------|
| 64: 7 vistas | 66: 5 vistas | Conteo LCD |
| 72: modo 1-pin | Codigo: AIN2=GPIO7 OUTPUT | Conflicto GPIO |
| 00: software cerrado | 71: funcionalidad nueva | Estado firmware |
| 72: LCD ignorado | 00: LCD estable | Estado LCD |
| 72: 105 tests | 68: 103 tests | Numero tests |

---

## 11. ERRORES EN HARDWARE (P0/P1)

### 11.1 Conflicto GPIO5/6 doble uso (CRITICO)

**Perfil 4:**
- GPIO5 = PIN_SALA (luz) Y PIN_BIN1 (ventilador DRV8833)
- GPIO6 = PIN_CUARTO (luz) Y PIN_BIN2 (ventilador DRV8833)
- GPIO4/7 compartidos DFPlayer UART y DRV8833 sin mutex

### 11.2 GPIO4/7 compartidos sin proteccion

DRV8833 configura GPIO4/7 como OUTPUT. Luego DFPlayer.begin() los reconfigura
como UART1. Si llega un comando de motor durante reproduccion de audio, corrompe
el UART1. No hay exclusion mutua en el codigo.

### 11.3 PIR deshabilitado

GPIO9 reasignado a LED VIVO. presRec=true hardcodeado. Las luces nunca apagan
por ausencia de persona. Esto invalida la automatizacion de luces combinadas.
---

## 12. HARDWARE: ESTADO ACTUAL CONFIRMADO POR SERIAL

### 12.1 Componentes que FUNCIONAN

| Componente | GPIO | Estado | Lectura |
|------------|------|--------|---------|
| ESP32-S3 N16R8 | COM9 CH343 | 8.7MB heap, 8.4MB PSRAM | OK |
| LCD1602 I2C | SDA17/SCL13 | 0x27, 50kHz | OK |
| Suelo ADC | GPIO15 | Resistivo | 3880 seco, 445 mojado |
| Nivel ADC | GPIO16 | Analogico | 1783 (62%) |
| LDR | GPIO3 | Divisor voltaje | 10% (oscuro) |
| Mando IR | GPIO12 | VS1838B | 21 teclas capturadas |
| PARO | GPIO10 | INPUT_PULLUP | Funciona |
| LED VIVO | GPIO9 | PWM | Funciona |
| S8050+bomba | GPIO4 | Transistor | Montado, sin probar con carga |

### 12.2 Componentes que NO funcionan

| Componente | Problema |
|------------|----------|
| DHT11 | NaN (cableado: 1->3V3, 2->GPIO14, 4->GND, R 10k) |
| DFPlayer | SD sin nombrar correctamente |
| IR receiver | 0 frames decodificados (necesita witness S8550) |
| DRV8833 | No recibido fisicamente |
| 74HC595 | Isaac no lo tiene |
| PIR | GPIO9 reasignado a LED VIVO |
| Fuente 5V/3A | Solo USB para bench |

### 12.3 Componentes sin probar

| Componente | Estado |
|------------|--------|
| DHT11 (cableado dado) | Cableado pero NaN |
| LDR (cableado dado) | Funciona parcialmente |
| 3 LED de cuarto | Cableados, sin probar |
| Motores/DRV | Sin cablear |

---

## 13. SISTEMA DE SEGURIDAD: 13 BARRERAS VERIFICADAS

| # | Barrera | Implementacion | Verificacion |
|---|---------|---------------|--------------|
| 1 | Boot OFF | digitalWrite antes de pinMode OUTPUT | static_assert |
| 2 | Watchdog 8s | esp_task_wdt hardware | test_native_safety.py |
| 3 | Safe mode <32KB | supervisarSalud() cada 2s | test_native_safety.py |
| 4 | Safe mode 3 reboots | RTC RAM counter | Setup check |
| 5 | Counter limpia 60s | contadorReiniciosEstabilizado | Logica firmware |
| 6 | 12 cmds/segundo | permitirComandoSerial() | test_native_firmware.py |
| 7 | Buffer 40 chars | Overflow -> discard hasta newline | test_native_firmware.py |
| 8 | Log circular 6 entradas | bufferErrores[] | Estructura firmware |
| 9 | Auto-riego nivel | leerNivelAgua() check | test_esqueleto_sim.py |
| 10 | Auto-vent DHT | Corte si leerAmbiente() falla | test_firmware_contract.py |
| 11 | Auto-luces LDR | Corte si leerLuz() falla | test_firmware_contract.py |
| 12 | Bomba 120s | verificarLimiteBomba() | test + native |
| 13 | Audio/SD off | MICROSD_HABILITADA=false | Compile flag |

**Veredicto:** Las barreras estan correctamente implementadas y verificadas por
4 metodos independientes (regex, C++, espejo Python, HIL fisico).

---

## 14. SISTEMA DE ERRORES: ARQUITECTURA

- **Buffer circular:** 6 entradas con timestamp, indices de escritura/lectura
- **Rate limiting:** cada origen avisa max 1 vez por 30 segundos
- **Diagnostico:** construirReporteDiagnostico() genera reporte completo
- **Total acumulado:** contador historico sin reset
- **Buffer estatico:** snprintf a buffer fijo (sin heap allocation)

**Debilidad:** registrarError() (linea 498) hace String concatenation en heap,
que puede fallar si el heap esta bajo (justo el escenario que genera errores).

---

## 15. SISTEMA DE AUTOMATIZACIONES: MAPA COMPLETO

### 15.1 Automatizaciones absolutas (5)

| # | Nombre | Trigger | Accion | Seguridad |
|---|--------|---------|--------|-----------|
| 1 | Riego automatico | Suelo < 35% | Bomba ON 120s max | Nivel > 20%, interlock |
| 2 | Riego combinado | Calor + tierra seca | Bomba ON 120s max | Nivel > 20% |
| 3 | Ventilador auto | Temp > 28C | Vent ON | Corte si DHT falla |
| 4 | Ventilador comb. | Calor + PIR presencia | Vent ON | Corte si DHT falla |
| 5 | Luces auto | Oscuridad + presencia | Luces ON | Corte si LDR falla |

### 15.2 Automatizaciones relativas (2)

| # | Nombre | Trigger | Timeout |
|---|--------|---------|---------|
| 1 | Auto-temporizado bomba | Encendido manual | 5 min auto-off |
| 2 | Auto-temporizado luces | Encendido manual | 10 min auto-off |

### 15.3 Problemas identificados

1. **Duplicacion:** verificarLucesAutomaticas() y verificarLucesCombinadas()
   controlan las mismas salidas (1 y 4) en cada loop.
2. **Bypass:** verificarAutomacionesRelativas() llama desactivarSalida() directamente,
   saltando ejecutarOrdenActuador().
3. **PIR hardcodeado:** presRec=true invalida toda automatizacion de presencia.
4. **DHT11 NaN:** Deshabilita ventilador automatico y riego combinado.
5. **LDR:** 10% oscuro puede no ser representativo del nivel real.

---

## 16. PROTOCOLO SERIAL: 26 COMANDOS

### 16.1 Comandos validos

`
PARO, PRENDER, APAGAR, ESTADO, DUMP, LED, RIEGO_ON, RIEGO_OFF,
IR_POWOFF, IR_POWON, IR_MUTE, VOL_MAS, VOL_MENOS, CH_MAS, CH_MENOS,
IR_KEY_x, CAL_SECO, CAL_HUMEDO, CAL_NIVEL, PINTEST, PINTEST_ALL,
IR_ESTADO, HISTORIAL, REPETIR (INALCANZABLE), SD_STATUS
`

### 16.2 Protecciones

- Rate limiting: 12 cmds/segundo (PARO exempt)
- Buffer: 40 chars, overflow -> descarta hasta newline
- Validacion: esComandoValido() contra COMANDOS_VALIDOS[]
- ACK/NACK: cada comando retorna exito/fallo con motivo

---

## 17. PANTALLA LCD: 7 VISTAS

| Vista | Contenido | Actualizacion |
|-------|-----------|---------------|
| 0 | Temperatura + humedad DHT | Cada 1.5s |
| 1 | Suelo ADC + % | Cada 1.5s |
| 2 | Luces sala/cuarto/inv | Cada 1.5s |
| 3 | Errores (ultimos 6) | Cada 1.5s |
| 4 | Estadisticas | Cada 1.5s |
| 5 | Perfil activo | Cada 1.5s |
| 6 | Overlay IR (timeout 3s) | Al presionar IR |

**Caracteristicas:**
- Shadow buffer [2][17] para rendering diferencial
- Auto-reconexion cada 30s si el bus se cae
- CGRAM icons (drop, sun, thermometer, level, voice)
- I2C escaneo 0x08-0x77 con auto-deteccion 0x27/0x3F

---

## 18. AUDIO JARVIS: 168 TRACKS

### 18.1 Estructura

- 21 eventos (CH_MENOS hasta TECLA_9)
- 4 variantes por evento (sin repeticion inmediata)
- 2 voces: Carlos (carpetas 01-21) y Karla (carpetas 51-71)
- Total: 21 x 4 x 2 = 168 MP3

### 18.2 Protecciones

- Cooldown 30s para alertas automaticas
- Prioridad de emergencia (detiene reproduccion actual)
- Sin repeticion inmediata de pista
- Volumen inicial: 18 (magic number, deberia ser constante)

### 18.3 Problemas

1. **DFPlayer silencioso:** SD con nombres Carlos/Karla en vez de 01-21/51-71
2. **GPIO4/7 compartidos** con DRV8833 sin exclusion mutua
3. **BUSY pin deshabilitado** (MP3_BUSY_PIN=-1) pero nota 72 lo describe cableado

---

## 19. IR REMOTO: 21 TECLAS

| Tecla | Codigo | Funcion |
|-------|--------|---------|
| CH- | 0x0045 | Volumen menos |
| CH | 0x0046 | Volumen mas |
| CH+ | 0x0047 | Página adelante |
| PREV | 0x0044 | Bomba ON/OFF |
| PLAY | 0x0040 | Luces ON/OFF |
| NEXT | 0x0043 | Ventilador ON/OFF |
| - | 0x0007 | LED ON/OFF |
| + | 0x0015 | Cultivo ON/OFF |
| EQ | 0x0009 | Estado resumido |
| 0-9 | 0x0016-0x000D | Funciones especiales |
| 100+ | 0x0019 | Cambio de voz |

**Persistencia:** 21 codigos + bitmask de aprendizaje en NVS.
**Rate limit:** 150ms para volumen, lockout 1.5s post-orden.
**Rechazo:** Duplicados y desconocidos contabilizados.

---

## 20. RECOMENDACIONES PRIORIZADAS

### Inmediatas (antes de feria)

1. **Resolver GPIO5/6:** Modo 1-pin DRV8833 (AIN2/BIN2 a GND)
2. **Reasignar DFPlayer UART:** GPIOs libres (40/41/42 propuestos)
3. **Cablear DHT11:** GPIO14 con pull-up 10k
4. **Renombrar SD DFPlayer:** 01..21/001..004.mp3
5. **Test IR hardware:** S8550 como witness
6. **Fix type mismatch:** bool tempAutoValida = leerAmbiente(...)

### Corto plazo

7. **Merge automatizaciones:** Unificar verificarLucesAutomaticas y Combinadas
8. **Fix bypass:** verificarAutomacionesRelativas debe pasar por ejecutarOrdenActuador
9. **Fix registrarEstadistica:** Usar parametro valor
10. **Fix REPETIR:** Agregar a COMANDOS_VALIDOS o eliminar handler
11. **Fix YAML:** Indentation en firmware-ci.yml linea 82
12. **Fix selftest PIR:** Hacer test realmente fallar

### Mediano plazo

13. **Eliminar ORIGEN_WIFI** de domus_types.h
14. **Corregir typos:** ADC_SUELRO, totalRiiegos, CAL_SUELDO
15. **Eliminar comentarios duplicados** en domus_drivers.h
16. **Marcar notas 50, 66, 67, 71** como obsoletas
17. **Fix generate_design.py:** XSS y PDF path
18. **Fix validate_project.py:** No silenciar directorios faltantes

---

## Relacionadas

- [[72 - Programa TODO Jarvis LCD DFPlayer e IR]]
- [[71 - Perfil CASA_FINAL_DRV8833_DFPLAYER]]
- [[70 - Mejoras firmware 2026-09-18]]
- [[69 - Placa real lado usable y restriccion de GPIO]]
- [[66 - Arquitectura final software hardware y funciones]]
- [[64 - Sesion fisica COM9 LCD IR sensores y bomba]]
- [[00 - Inicio]]