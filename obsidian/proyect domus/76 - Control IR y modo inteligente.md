---
estado: vigente
fecha: 2026-09-21
autoridad: 76
tipo: control-ir-modo-inteligente
reemplaza: notas parciales de 66 y 72
---

# 76 - Control IR y modo inteligente

> [!IMPORTANT]
> Esta nota define el mapeo final del mando IR, el diagnostico robusto,
> y el modo autonomo verdaderamente inteligente.

## Mando IR - Mapeo final (21 teclas)

### Teclas de FUNCION (arriba - configuracion, no encienden nada)

| Tecla | Codigo | Funcion | Audio Jarvis |
|-------|--------|---------|--------------|
| CH- | 0x45 | Modo manual | Carpeta 01 (sistema listo) |
| CH | 0x46 | Spare ON/OFF (GPIO6; LCD quemado, nota 80) | Carpeta 02 |
| CH+ | 0x47 | Modo automatico | Carpeta 01 (sistema listo) |
| ANTERIOR | 0x44 | Pagina LCD anterior | Sin audio |
| PLAY | 0x43 | Silencio Jarvis ON/OFF | Sin audio (toggle) |
| SIGUIENTE | 0x40 | Pagina LCD siguiente | Sin audio |
| VOL- | 0x07 | Bajar volumen | Sin audio |
| VOL+ | 0x15 | Subir volumen | Sin audio |
| EQ | 0x09 | **DIAGNOSTICO ROBUSTO** | Carpeta 20 (diagnostico) |

### Teclas NUMERICAS (acciones directas)

| Tecla | Codigo | Funcion | Audio Jarvis |
|-------|--------|---------|--------------|
| 0 | 0x16 | Todo apagado (emergencia) | Carpeta 15 (emergencia) |
| 1 | 0x0C | Casa ON/OFF | 04/05 (casa on/off) |
| 2 | 0x18 | Porche ON/OFF | 06/07 (porche on/off) |
| 3 | 0x5E | Cultivo ON/OFF | 08/09 (cultivo on/off) |
| 4 | 0x08 | **Todas las luces ON/OFF** | 04+06+08 o 05+07+09 |
| 5 | 0x1C | Riego ON/OFF | 10/11 (riego on/off) |
| 6 | 0x5A | Leer temperatura | 18 (temp alta) o 21 (estado) |
| 7 | 0x42 | Leer humedad | 13 (tierra humeda) o 16 (error) |
| 8 | 0x52 | Leer suelo + deposito | 12 (tierra seca) o 14 (agua baja) |
| 9 | 0x4A | Estado completo | 21 (estado) |

### Teclas especiales

| Tecla | Codigo | Funcion | Audio Jarvis |
|-------|--------|---------|--------------|
| 100+ | 0x19 | Alterna voz Carlos/Karla | 01 (sistema listo, voz nueva) |
| 200+ | 0x0D | Rearme seguro | 20 (diagnostico) o 15 (bloqueado) |

## Diagnostico robusto (tecla EQ)

Al presionar EQ, el firmware ejecuta:

### Paso 1: Leer todos los sensores
`
DHT11  → temp, humedad
Suelo  → crudo, porcentaje
Nivel  → crudo, porcentaje
LDR    → crudo, porcentaje
IR     → teclas aprendidas / 21
DFPlayer → detectado / no detectado
PARO   → presionado / libre
`

### Paso 2: Mostrar en LCD (2s por sensor)
`
Pantalla 1: "SENSOR CHECK"
Pantalla 2: "DHT11: OK 25C 60%"
Pantalla 3: "Suelo: OK 42%"
Pantalla 4: "Nivel: OK 62%"
Pantalla 5: "LDR: OK 15%"
Pantalla 6: "IR: 21/21"
Pantalla 7: "DFP: OK | PARO: OK"
Pantalla 8: "RESUMEN: 7/7 OK"
`

Si hay fallos:
`
Pantalla: "DHT11: FAIL NaN"
Pantalla: "Nivel: FAIL flotante"
Pantalla: "RESUMEN: 5/7 OK"
`

### Paso 3: Audio Jarvis segun resultado

| Resultado | Audio |
|-----------|-------|
| Todos OK | Carpeta 20 variante 1-2: "Todos los sensores OK" |
| Algunos FAIL | Carpeta 16: "Diagnostico requerido" + cada fail repite |
| Todos FAIL | Carpeta 16 variante 3-4: "Sensor sin respuesta" |

### Paso 4: Reporte por Serial
`
DIAGNOSTICO;DHT11=OK/25.3/60;SUELO=OK/42;NIVEL=OK/62;LDR=OK/15;IR=21/21;DFP=OK;PARO=OK;RESUMEN=7/7
`

## Modo inteligente (autonomo)

### Principios

1. **Coordinacion**: todos los sensores se leen juntos, las decisiones se toman juntas
2. **Limites diarios**: bomba max 30 min/dia, luces cultivo max 16h/dia
3. **Cooldowns**: bomba min 5 min OFF entre ciclos, luces min 30s entre toggles
4. **Degradacion**: si un sensor falla, usar ultima lectura conocida + timeout
5. **Audio contextual**: cada accion tiene su frase Jarvis

### Estado compartido

`cpp
struct EstadoInteligente {
  // Limites diarios
  unsigned long bombaMsHoy;
  unsigned long lucesCultivoMsHoy;
  
  // Cooldowns
  unsigned long ultimoCicloBombaMs;
  unsigned long ultimoToggleLucesMs;
  
  // Contexto
  bool oscuridad;
  bool presencia;
  bool calor;
  bool tierraSeca;
  bool aguaBaja;
  
  // Salud sensores
  bool dht11OK;
  bool sueloOK;
  bool nivelOK;
  bool ldrOK;
  unsigned long ultimoDHT11ValidoMs;
};
`

### Decision coordinada

`
SI oscuridad Y presencia:
  → Casa ON (si no manual OFF)
  
SI oscuridad Y NOT presencia:
  → Casa OFF (ahorrar energia)
  
SI oscuridad (cualquier cosa):
  → Cultivo ON (fotoperiodo)
  
SI tierraSeca Y aguaOK Y bombaNoEnCooldown Y bombaMsHoy < 30min:
  → Bomba ON
  
SI calor Y presencia:
  → (ventilador eliminado - sin accion)
  
SI aguaBaja:
  → Bomba OFF (interlock)
  
SI sensorFalla:
  → Mantener estado actual + alerta
`

### Limites diarios

| Recurso | Limite | Accion al llegar |
|---------|--------|------------------|
| Bomba | 30 min/dia | Apagar, alerta "limite diario" |
| Luces cultivo | 16 horas/dia | Apagar, alerta "fotoperiodo completo" |
| Luces casa | Sin limite | Se apagan por PIR/LDR |
| Luces porche | Sin limite | Se apagan por LDR |

### Degradacion inteligente

| Sensor falla | Que hacer |
|--------------|-----------|
| DHT11 | Usar ultima temp conocida < 5 min; si > 5 min, apagar automaciones que dependen de temp |
| Suelo | Apagar bomba automatica, mantener manual |
| Nivel | Apagar bomba inmediatamente (ya existe) |
| LDR | Mantener estado actual de luces, no cambiar |
| IR | Solo control por Serial |
| DFPlayer | Sin audio, todo lo demas funciona |

## Relacionadas

- [[74 - Hardware final y mapa GPIO v5]]
- [[75 - Audio Jarvis remodelado]]
- [[77 - Plan firmware final v5]]