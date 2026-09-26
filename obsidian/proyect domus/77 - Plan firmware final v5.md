---
estado: vigente
fecha: 2026-09-21
autoridad: 77
tipo: plan-firmware-v5
depends: 74, 75, 76
---

# 77 - Plan firmware final v5

> [!IMPORTANT]
> Esta nota documenta TODOS los cambios que se aplicaran al firmware
> casa_inteligente_v4.ino y headers. Ejecutar en orden.

## Resumen de cambios

| Tipo | Cantidad |
|------|----------|
| Eliminaciones | 5 |
| Modificaciones | 8 |
| Nuevos | 4 |
| **Total** | **17** |

## FASE 1: Eliminaciones

### 1.1 ELIMINAR verificarVentiladorCombinado()
- **Archivo:** casa_inteligente_v4.ino:1551-1571
- **Razon:** Ventilador muerto, funcion sin uso

### 1.2 ELIMINAR verificarVentiladorAutomatico()
- **Archivo:** casa_inteligente_v4.ino:1631-1664
- **Razon:** Ventilador muerto, funcion sin uso

### 1.3 ELIMINAR verificarLucesAutomaticas()
- **Archivo:** casa_inteligente_v4.ino:1668-1717
- **Razon:** Duplica verificarLucesCombinadas(). Mantener solo Combinadas.

### 1.4 ELIMINAR ventilador del loop()
- **Archivo:** casa_inteligente_v4.ino:2707-2708
- **Lineas:** verificarVentiladorAutomatico() y verificarVentiladorCombinado()
- **Razon:** Funciones eliminadas

### 1.5 ELIMINAR VENT_ON/VENT_OFF de COMANDOS_VALIDOS
- **Archivo:** casa_inteligente_v4.ino:1765-1767
- **Razon:** No hay ventilador

## FASE 2: Modificaciones

### 2.1 Renombrar salidas
- **Archivo:** casa_inteligente_v4.ino:175-178
- **De:** "Bomba", "Luz Sala", "Luz Cuarto", "Ventilador", "Luz Inv."
- **A:** "Bomba", "Casa", "Porche", "Cultivo", "Spare"
- **Nota:** Casa = sala+ducha en paralelo (GPIO5). Spare = index 4 sin uso fisico.

### 2.2 Actualizar MAPA_CASA
- **Archivo:** casa_inteligente_v4.ino:222-226
- **De:** {4, 5, 6, 7, 8}
- **A:** {4, 5, 8, 7, 8} (bomba, casa, porche, cultivo, spare)
- **Nota:** Se elimina GPIO6 (cuarto) y GPIO7 (vent). Se reasignan indices.

### 2.3 Actualizar static_asserts
- **Archivo:** casa_inteligente_v4.ino:229-236
- **Razon:** Reflejar nuevos pines

### 2.4 Fix bool tempAutoValida (BUG CRITICO)
- **Archivo:** casa_inteligente_v4.ino:2703-2704
- **De:** float tempAutoValida = false; (recibe humedad, no bool)
- **A:** bool tempAutoValida = leerAmbiente(tempAutoC, humAutoAire);
- **Razon:** El return value de leerAmbiente se descarta, humedad se trae como bool

### 2.5 DFPlayer UART a GPIO11/18
- **Archivo:** casa_inteligente_v4.ino:2538-2546
- **De:** transporteDFPlayer.begin(4, 7, MP3_BUSY_PIN)
- **A:** transporteDFPlayer.begin(11, 18, MP3_BUSY_PIN)
- **Razon:** GPIO4/7 estan ocupados por bomba y cultivo. GPIO11/18 son botones MODO/SILENCIO que no se usan con mando IR.

### 2.6 Actualizar domus_drivers.h
- **Archivo:** domus_drivers.h:62-69
- **Cambios:** Comentar DRV8833_PIN_BIN1/BIN2 (GPIO5/6 ya no son ventilador)

### 2.7 Actualizar nombres en LCD
- **Archivo:** domus_pantalla.h
- **Cambios:** "Luz Sala" → "Casa", "Luz Cuarto" → "Porche", "Luz Inv." → "Cultivo"

### 2.8 Actualizar tecla IR ventilador → "todas luces"
- **Archivo:** domus_ir_casa.h:29 (enum N_4)
- **De:** N_4 = ventilador
- **A:** N_4 = todas_luces

## FASE 3: Nuevas funcionalidades

### 3.1 Modo inteligente (EstadoInteligente struct)
- **Archivo:** casa_inteligente_v4.ino (nueva seccion)
- **Contenido:**
  - Struct EstadoInteligente con limites diarios, cooldowns, contexto
  - Funcion revisarModoInteligente() que lee todo y decide
  - Coordinacion entre riego, luces, y sensores
  - Degradacion inteligente por fallo de sensor

### 3.2 Limites diarios
- **Bomba:** max 30 min/dia
- **Luces cultivo:** max 16h/dia
- **Cooldown bomba:** min 5 min OFF entre ciclos
- **Cooldown luces:** min 30s entre toggles

### 3.3 Diagnostico robusto (tecla EQ)
- **Archivo:** casa_inteligente_v4.ino (nueva funcion)
- **Funcion:** diagnosticarSensores()
  1. Leer todos los sensores
  2. Mostrar en LCD ciclo por ciclo (2s cada uno)
  3. Reproducir audio Jarvis segun resultado
  4. Reportar por Serial

### 3.4 Todas las teclas con audio
- **Archivo:** casa_inteligente_v4.ino (en ejecutarTeclaIRCasa)
- **Cambio:** Agregar anunciarJarvis() a cada tecla que no lo tenga
- **Nota 76:** Mapeo completo tecla → evento Jarvis

## Archivos afectados

| Archivo | Cambios |
|---------|---------|
| casa_inteligente_v4.ino | Eliminar 3 funciones, modificar 8 secciones, agregar 4 nuevas |
| domus_drivers.h | Comentar BIN1/BIN2 |
| domus_ir_casa.h | Renombrar N_4 |
| domus_pantalla.h | Actualizar nombres de salidas |
| domus_jarvis_audio.h | Sin cambios (ya funcional) |
| domus_dfplayer.h | Sin cambios |
| domus_calibration.h | Sin cambios |
| domus_types.h | Sin cambios |

## Orden de compilacion

1. Modificar domus_drivers.h (comentar BIN1/BIN2)
2. Modificar domus_ir_casa.h (renombrar N_4)
3. Modificar domus_pantalla.h (nombres)
4. Modificar casa_inteligente_v4.ino (eliminaciones + modificaciones)
5. Agregar nuevas funciones (modo inteligente, diagnostico)
6. Compilar con -DDOMUS_PERFIL_CASA=3
7. Verificar que compila sin errores
8. Ejecutar tests

## Relacionadas

- [[74 - Hardware final y mapa GPIO v5]]
- [[75 - Audio Jarvis remodelado - Carlos Karla y SD]]
- [[76 - Control IR y modo inteligente]]
- [[73 - Auditoria Profunda Completa del Proyecto]]