---
estado: vigente
fecha: 2026-09-26
autoridad: 82
tipo: cierre-jurado-documentacion
depends: 79, 80, 81
---

# 82 — Cierre: bomba GPIO17, Jarvis PC y documentación final

> [!IMPORTANT]
> Sesión de cierre: la bomba se recableó a **GPIO17** y quedó verificada en
> vivo, la consola Jarvis/NEXUS se pulió (voz, SFX, diagnóstico hablado,
> latencia de audio mínima) y se cerró la documentación para la **presentación
> al jurado** del 26 sep 2026.

## 1. Bomba en GPIO17 (recableado por Isaac)

- `MAPA_CASA.bomba = 17` (antes GPIO4); struct con `sda = -1` (centinela
  "pin libre": LCD quemado y MP3 fuera) — `firmware/casa_inteligente_v4/casa_inteligente_v4.ino`.
- `static_assert` de mapa y salidas actualizados (`bomba==17`, `sda==-1`);
  salidas = `[17, 5, 8, 7, 6]`.
- `DIAGNOSTICO` reporta `BOMBA_ETAPA=GPIO_DIRECTO`; apaga luces al regar;
  timeout 120 s.
- **Fix de PINTEST**: `procesarPinTest` y `PINTEST_ALL` ahora restauran las 5
  salidas con su estado real y reinician AIN1/AIN2 del DRV8833 (antes dejaba
  los pines en `INPUT` → bomba/luces muertas hasta reinicio).
- **DFPlayer sigue deshabilitado**: su RX era GPIO17 y hoy ese pin es la
  bomba. Reactivar exige recablear RX/TX (GPIO1/2 libres); TX comparte
  GPIO18 con el botón DEMO. No tocar sin coordinar.

### Verificación en vivo (COM9)

```
RIEGO_ON  → SALIDA Bomba ENCENDIDO (GPIO verificado) · Casa/Porche apagados
ACK;RIEGO_ON;1
ESTADO    → Bomba=1 Casa=0 Porche=0 Spare=0
RIEGO_OFF → ACK;RIEGO_OFF;0
```

## 2. Consola Jarvis/NEXUS (`tools/jarvis_pc/jarvis.html`)

Interfaz WebUSB de PC presentada al jurado (100% local, sin servidor):

- **Subtítulos y voz TTS en español**: `pickVoice()` puntúa voces
  (neural/natural > Google > Microsoft; es-MX > es-ES); clic en "Voz" cicla
  las voces del PC con frase de prueba.
- **Diagnóstico hablado**: panel con **Diagnóstico / Prueba guiada /
  Recuperar** (microSD, micrófono y barrido de pines se quitaron del panel;
  los comandos siguen disponibles por monitor serie). `diagPending` traduce
  `DIAGNOSTICO;…`, `ESTADO;…`, `PRUEBA;FIN`, `PINTEST;FIN`, `MIC;ON`,
  `ACK;SD_PRUEBA;ENCOLADA` y `NACK;…` a frases habladas.
- **SFX estéreo** (Web Audio por las bocinas del PC): blip al pulsar,
  chime L→R al conectar, doble ping en ACK, zumbido en NACK/error y alarma
  alternada en PARO. Anti-spam ~170 ms.
- **Latencia de audio mínima**: sin espera cuando no hay frase en vuelo
  (los 50 ms solo aplican al cancelar una frase anterior, bug Chrome);
  prewarm de TTS + AudioContext al primer gesto; keep-alive cada 10 s que
  reanuda la síntesis pausada.
- **Panel 3 botones** + `btnEstado` habla el estado; orbe "NODO JARVIS".

Validación: `node tools/jarvis_pc/smoke_jarvis.js` → **64 passed, 0 failed**
(DOM/TTS/Serial simulados); verificación en Edge headless sin pageerrors.

## 3. Pruebas y validación

| Suite | Resultado |
|---|---|
| `python -m unittest discover -s firmware/tests` | **107 passed, 0 failed, 1 skipped** (HIL sin puerto) |
| `node tools/jarvis_pc/smoke_jarvis.js` | **64 passed, 0 failed** |
| Última carga HIL COM9 | 8/8 |
| Compilación producto | 434.341 bytes (FQBN N16R8) |

> [!NOTE]
> El g++ nativo de esta máquina (WinGet GCC 16.1.0) quedó inestable en el
> entorno: `cc1plus` falla de forma intermitente incluso con `int main(){}`.
> Las suites nativas se compilan en CI Ubuntu; esto no es un fallo del código.

## 4. Documentación final (pasada completa)

- **`README.md` raíz**: reescrito — qué es el proyecto, diagrama de bloques,
  cómo funciona (riego/AUTO invernadero/PARO), consola Jarvis, estado final,
  estructura del repo y comandos de validación (números exactos: 107 + 64).
- **`docs/ESTADO_ACTUAL.md`**: actualizado a hardware real (bomba 17, LEDs
  5/8/6, DFPlayer/LCD fuera) y al cierre del 26 sep.
- **`docs/ENTREGA_FINAL.md`**: Jarvis ahora incluye la consola PC con voz;
  alcance y pendientes al día.
- **`docs/INDICE.md`**: ruta nueva a `tools/jarvis_pc/` y a esta nota.
- **Nota 18 (manual maestro)**: tabla pin por pin corregida al mapa vigente
  (suelo 15, nivel 16, micOff 9, DEMO 18, IR 12 **reservado**, bomba 17,
  LCD descartado, PIR fuera); diagrama y controles físicos al día.
- **Wikilinks rotos** de la nota 75 corregidos en 74/76/77/78.
- **Validadores**: `validate_project.py` actualizado al mapa actual
  (`EXPECTED_MAPA_PINS`, salidas `[17,5,8,7,6]`, `sda=-1` se salta la guía);
  `AUDIO_CANDIDATO_HABILITADO = false` literal en `domus_drivers.h`
  (coherente con el módulo deshabilitado).

## 5. Estado de entrega

- Repositorio: commit final con firmware (bomba 17 + fix PINTEST), tests,
  Jarvis PC (`tools/jarvis_pc/`), notas 74-78/81/82, README y docs.
- Pendiente real (no bloquea): calibración definitiva `CAL_SECO/CAL_HUMEDO`,
  pruebas en vivo de microSD, mediciones eléctricas, y — si se decide —
  reactivar audio DFPlayer con recableado.
