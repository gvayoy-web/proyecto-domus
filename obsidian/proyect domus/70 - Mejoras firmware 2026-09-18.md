# 70 - Mejoras de firmware 2026-09-18 (COM9)

**Fecha:** 2026-09-18
**Placa:** ESP32-S3 N16R8 en COM9 (CH343)
**Perfil:** `BANCO_COMPLETO_S8050_IR`
**Autoridad:** código fuente + pruebas Python

## Resumen de mejoras aplicadas

### 1. DIAGNOSTICO ampliado con valores crudos y calibración
- `construirReporteDiagnostico` ahora incluye:
  - `ADC_SUELRO`, `ADC_NIVEL`, `ADC_LDR` — lecturas crudas de los sensores
  - `CAL_SECO`, `CAL_HUMEDO`, `CAL_OSCURO`, `CAL_CLARO`, `CAL_NIVEL_MIN` — valores de calibración actuales
- Esto facilita la calibración sin necesidad de abrir el Monitor Serial y leer valores manualmente
- Buffer aumentado de 768 a 1024 bytes para accommodate los nuevos campos

### 2. Comando PINTEST_ALL
- Nuevo comando Serial: `PINTEST_ALL`
- Escanea todos los GPIO del lado usable (3-18) en una sola ejecución
- Muestra crudo, pull-up, pull-down y estado FLOTANTE/CONECTADO para cada pin
- Útil para diagnóstico rápido de todo el costado accesible
- Funciona sin necesidad de especificar cada pin individualmente

### 3. Optimización de automatizaciones combinadas
- `verificarRiegoAutomaticoCombinado` y `verificarVentiladorCombinado` ahora comparten una sola lectura de DHT
- Se agregó `leerAmbiente(float&, float&)` a las declaraciones adelantadas
- Elimina una lectura redundante del DHT11 por ciclo (el sensor es lento ~1s/lectura)
- La temperatura se lee una vez en `loop()` y se pasa a ambas funciones

### 4. Corrección de indentación en domus_pantalla.h
- `if (!splashHecho_)` en `PantallaFinal::tick()` estaba mal indentado
- Corregido para mantener consistencia con el resto del código

### 5. Corrección de import en test_pantalla_final.py
- Se agregó `sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))`
- `test_pantalla_final` ahora pasa correctamente (era el único test que fallaba por import)
- Total: 49/49 pruebas pasando

### 6. Declaraciones adelantadas de funciones de sensor
- Se agregaron `leerHumedad`, `leerLuz` y `leerAmbiente` a las declaraciones adelantadas
- `leerAmbiente` era usada por `verificarRiegoAutomaticoCombinado` y `verificarVentiladorCombinado` sin declaración previa

## Estado actual del firmware en COM9 (antes de recompilar)

| Métrica | Valor |
|---|---|
| Perfil | BANCO_COMPLETO_S8050_IR |
| LCD | LCD 0x27 (PASS, 50 kHz) |
| IR | 21 códigos capturados (0/21 aprendidas en NVS) |
| DHT11 | NaN (suspendido tras 3 fallos) |
| Suelo | Fuera de rango (sensor no conectado o calibración faltante) |
| Nivel | 1186 ADC (provisional, 600=0%, 2500=100%) |
| LDR | 10% (oscuro) |
| PIR | 1 (presencia detectada) |
| Salidas | Bomba=0, Sala=1, Cuarto=0, Vent=0, Inv.=1 |
| Memoria | 8.7MB heap libre, 8.4MB PSRAM libre |
| Errores | 82 total acumulados |
| Calibración | PROVISIONAL |
| Audio | APLAZADO |

## Pendientes de hardware (no se resuelven con firmware)

- DHT11 NaN: necesita revisar cableado (pies 1→3V3, 2→GPIO14, 4→GND, resistencia 5.1-10kΩ entre 1 y 2)
- Suelo fuera de rango: necesita conectar el sensor resistivo correctamente o calibrar
- IR sin aprender: 21 teclas necesitan ser aprendidas con `IR_GRABAR_0` a `IR_GRABAR_20`
- Calibración provisional: necesita valores reales de seco/húmedo/oscuro/claro/nivel
- S8050 + bomba: necesita verificar etapa transistoraria

## Relacionadas

- [[69 - Placa real lado usable y restriccion de GPIO]]
- [[64 - Sesion fisica COM9 LCD IR sensores y bomba]]
- [[66 - Arquitectura final software hardware y funciones]]
- [[65 - Arquitectura de audios Jarvis con DFPlayer]]
- [[63 - Auditoria total de Obsidian y estado real]]
