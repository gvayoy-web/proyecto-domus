# 72 - Programa TODO: Jarvis, LCD, DFPlayer e IR (sesión completa)

**Fecha:** 2026-09-20
**Placa:** ESP32-S3 N16R8 en COM9 (CH343)
**Firmware en placa al cierre:** `firmware/domus_todo/domus_todo.ino` (todo-en-uno)
**Autoridad:** compilación + subida por `arduino-cli` con hash verificado + Serial 115200

## 1. Punto de partida y corrección a la nota 71

- La nota 71 asume **74HC595**: Isaac confirma que **no lo tiene**. El perfil 4
  como está documentado no aplica. Se adopta el **perfil 3
  (`BANCO_COMPLETO_S8050_IR`)** como base válida + programa TODO propio.
- El 74HC595 de inventario (nota 01) queda como opcional, no requerido.
- `DOMUS_PERFIL_CASA` sigue en 4 por defecto en el `.ino`; lo cargado en COM9
  se compiló con `-DDOMUS_PERFIL_CASA=3`. No flashear el default sin revisar.

## 2. Modelo 3D `modelo_3d_con_componentes.html` (estaba negro)

Causa: ~12 errores de sintaxis JS que mataban el script antes de dibujar.
Corregidos y verificados con Playwright (captura en `output/modelo_3d_fixed.png`):

- `material="..."` / `layer="..."` con `=` en vez de `:` (5 piezas).
- Sintaxis tipo TS en JS: `let rot:150`, `for(let x:0`, `dist:40`,
  `tourIdx:0`, `lastFrame:0`, `ctx.lineWidth:1`, `ctx.lineWidth:2`,
  `ctx.globalAlpha:1`.
- `const canvas/ctx/faces` inexistentes → se agregaron (`faces` del modelo
  interactivo, `COLORS` hex→arrays RGB).
- `let polys=[],centers=[]` sombreaba el global `centers` (sin tags ni clicks).
- `#views` sin id; `#all`/`#reset` inexistentes → guards nulos.
- `drawTags()` rota → reescrita simple.
- Resultado: `node --check` limpio, `33 piezas / 25 cables`, sin errores de consola.

## 3. Hallazgo de seguridad en firmware (corregido)

- `domus_drivers.h`: `driverMotoresListo() { return true; }` puenteaba la
  puerta F1 (motores nunca bloqueados). Cambiado a
  `return DRIVER_MOTORES_LISTO;`.
- Conflicto perfil 4 sin resolver: DFPlayer UART (GPIO4/7) choca con DRV8833
  (GPIO4/7) y GPIO5/6 son DRV y luces a la vez. Por eso el programa TODO usa
  el DRV en **modo 1-pin** (AIN2/BIN2 a GND): bomba=GPIO4, vent=GPIO7.
- Test suite: 105 passed, 1 fail preexistente
  (`test_perfil_final_drv8833_dfplayer` espera `SHIFT74HC595` que el `.ino`
  no tiene — contradicción nota 71 vs código, no causada en esta sesión).

## 4. Sensores verificados por Serial

| Sensor | Medición | Veredicto |
|---|---|---|
| Suelo GPIO15 | ~3880 estable en tierra seca; 2821 en otra muestra; 445 mojado | ✅ Funciona. Calibrar `CAL_SECO`/`CAL_HUMEDO` (refs 2800/1200 no calzan con su tierra) |
| Nivel GPIO16 | 1783 (62%) bajando 62→57 en 6 s (secándose) | ✅ Funciona. Calibrar `CAL_NIVEL` seco/mojado |
| Desconectados | Suelo 74→89 errático, nivel 24→21 deriva | ⚠️ Pin flotante inventa % "válidos": nunca `RIEGO_AUTO` sin sensores |
| PINTEST GPIO15 | CRUDO 3880, PULLUP 3930, PULLDOWN 3340 | Pin driven (no flotante) = cableado bien |
| PIR GPIO9 | Ignorado por decisión de Isaac | Pin reasignado a LED VIVO |

## 5. LCD: reglas de Obsidian, parpadeo y soak-test nocturno

Reglas confirmadas (notas 00/39/57/63/64/66/70): I2C **50 kHz**, **sin
`clear()`**, **escritura diferencial** con sombra, LCD a **3V3**, SDA/SCL
antes de encender, contraste con potenciómetro azul (nota 37).

- Culpa propia encontrada: mi `lcdShow` reescribía 32 celdas cada 1.5 s →
  parpadeo. Reescrito a diferencial por carácter. Subido y verificado.
- Brillo máximo: **no es software** (backlight del PCF8574 es on/off).
  Checklist dado: pot azul, jumper BL, caída del riel 3V3 (DFPlayer+LEDs por
  jumpers finos), filtro 10 µF + 104 pegados al backpack.
- Soak `firmware/lcd_soak/`: 9600 celdas en 50/75/100 kHz, **0 errores**,
  LCD 0x27 siempre detectado, bus estable 5 min. Veredicto: se queda 50 kHz.
  El parpadeo restante es hardware (contactos/alimentación), no código.
- Estado: LCD actualmente **ignorado** por Isaac; firmware corre headless.

## 6. Jarvis: DFPlayer MP3-TF-16P + parlantes + 2 LED azules + IR

- Módulo identificado por foto: MP3-TF-16P. Solo se cablean VCC/RX/TX/BUSY/
  SPK_1/SPK_2/GND/SD. USB±, DAC_*, ADKEY_*, IO_* quedan al aire.
- RX = oreja (GPIO18→R1k→RX), TX = boca (TX→GPIO11). BUSY = chismoso
  (LOW mientras suena) → R10k → S8050 → LED HABLA (sin GPIO).
- Parlantes: **serie, nunca paralelo**. 4W + mini en cadena
  `SPK_1→4W→mini→SPK_2`. Los minis resultaron **8Ω 1W** (no 1Ω): directo ok,
  pero `VOL` ≤ 20 o se vuela el de 1W. Hay 5 de repuesto.
- LED VIVO: GPIO9 PWM → R1k → 2º S8050 → LED (PIR ignorado). Respira en
  reposo, excitado al hablar/mando, parpadeo rápido en PARO, triple
  parpadeo con cada tecla IR.
- Botones MODO/SILENCIO **desconectados** (GPIO11/18 = UART DFPlayer);
  el mando los reemplaza (CH=página, PLAY=silencio).
- S8050: cara plana al frente, patas abajo = 1 emisor (GND), 2 colector
  (cátodo LED), 3 base (R). Verificar con multímetro (~0.7V base-emisor).
- LED: pata larga ánodo, corta cátodo; lado plano del cuerpo = cátodo.
- VS1838B: domo redondo al mando; de izq a der OUT→GPIO12, GND, VCC→3V3.

## 7. Silencio del DFPlayer (diagnóstico abierto)

- ESP32→DFP verificado (`DFP>carpeta 21 pista 1 [9] sonando` sale en Serial).
- SD del usuario: `Carlos/01..21/001..004.mp3` + `Karla/...` (estructura sana,
  `jarvis_sd/` ignorado). Para la SD física renombrar:
  `Carlos/*` → `01..21`, `Karla/*` → `51..71`, FAT32, puesta **antes** de
  encender. Con nombres `Carlos/Karla` los comandos por carpeta fallan
  (explica: autoplay del arranque sonó, comandos callan).
- Sospechas pendientes: cable GND DFP↔ESP32, RX/TX cruzados al revés,
  SD metida después de encender, o caída de los "4.4V" bajo carga.
- Mitigación de ruido IR flotante: código 0x0 ignorado + NACK desconocidas
  max 1/2 s. Agregados contadores (`IR_ESTADO`: total/conocidas/
  desconocidas/último) para diagnosticar sin timing.
- Estado IR al cierre: `total=0` — el ESP32 no ha decodificado ni una trama.
  Siguiente paso: testigo hardware con S8550 (OUT→R10k→base, LED en
  colector) para separar "receptor sordo" de "cable GPIO12".

## 8. Archivos nuevos/modificados en la sesión

- `firmware/domus_todo/domus_todo.ino` (nuevo, en placa)
- `firmware/test_suelo_drv_ir/` (nuevo, test previo; en `build/`)
- `firmware/lcd_soak/` (nuevo, soak nocturno; en `build/`)
- `firmware/CONEXIONES_PROGRAMA_TODO.md` (nuevo, diagrama + eléctrico)
- `hardware/planos/modelo_3d_con_componentes.html` (corregido, sin trackear)
- `firmware/casa_inteligente_v4/domus_drivers.h` (fix seguridad F1)
- `output/modelo_3d_fixed.png` (captura de verificación)

## 9. Pendientes

1. Armar LED VIVO y leer triple parpadeo con tecla 5.
2. Testigo S8550 si el VIVO no reacciona (separar receptor vs GPIO12).
3. SD con layout 01–21/51–71 + reboot + tecla 9.
4. Confirmar GND DFP↔ESP32 y orientación RX/TX; medir "4.4V" bajo carga.
5. Botón PARO (GPIO10→GND). Reconectar suelo+nivel y calibrar.
6. Pin "ult" del DRV8833 sin identificar (al aire). Impedancia del 4W sin confirmar.
7. Motores/DRV, LDR, DHT11, 3 LED de cuarto: cableado dado, sin probar.
8. Decidir destino de `DOMUS_PERFIL_CASA=4` por defecto (no flashear a ciegas).

## Relacionadas

- [[71 - Perfil CASA_FINAL_DRV8833_DFPLAYER]]
- [[70 - Mejoras firmware 2026-09-18]]
- [[69 - Placa real lado usable y restriccion de GPIO]]
- [[64 - Sesion fisica COM9 LCD IR sensores y bomba]]
- [[39 - Inventario fotografiado y pines visibles]]
- [[01 - Inventario confirmado]]
- [[18 - Manual maestro de conexiones pin por pin]]
