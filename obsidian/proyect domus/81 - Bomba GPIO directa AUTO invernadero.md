---
estado: vigente
fecha: 2026-09-24
autoridad: 81
tipo: sesion-invernadero-bomba-gpio
depends: 76, 79, 80
---

# 81 — Bomba GPIO directa, AUTO invernadero y Obsidian

> [!IMPORTANT]
> Sesión de feria: bomba por GPIO (sin DRV en el camino), modo AUTO =
> invernadero (riego por suelo), transición de luces al cambiar de modo,
> y bomba ON apaga todas las luces.

## Cambios de firmware (`casa_inteligente_v4.ino`)

### 1. Bomba directa en GPIO (perfil 4)

- `BOMBA_DIRECTA_S8050` ahora es `true` también en
  `PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER` (además del perfil 3 S8050).
- Arranque: GPIO4 (`MAPA_CASA.bomba`) sale como `OUTPUT` normal
  (`pinEsPuentH` se anula cuando hay bomba directa).
- `DIAGNOSTICO` reporta `BOMBA_ETAPA=GPIO_DIRECTO` (antes `DRV`).
- Gate de seguridad: con `BOMBA_DIRECTA_S8050` no se exige
  `driverMotoresListo()` para `RIEGO_ON` / AUTO.
- Log de arranque: “Bomba GPIO directa bloqueada al arrancar; usa RIEGO_ON o modo AUTO”.
- Timeout 120 s (`TIEMPO_MAXIMO_BOMBA_MS`) sigue activo.

> [!WARNING]
> Bomba cableada al ESP32 sin DRV8833. Solo usar con fuente/carga apta
> para GPIO o transistor; no es el camino “DRV validado F1”.

### 2. Bomba ON → apaga todas las luces

En `ejecutarOrdenActuador`, si la orden enciende la bomba (índice 0) y
queda ON, se apagan Casa/Porche/Spare con origen `ORIGEN_SISTEMA` y
nombre `BOMBA_ON_APAGA_LUCES` (sin tocar cultivo, sin etapa).

### 3. Modo AUTO = invernadero (no show de luces)

| Antes | Ahora |
|-------|--------|
| AUTO parpadeaba luces para siempre | Transición ~3 s de parpadeo y luego **luces OFF** |
| Sin riego automático | `automatizarInvernadero()` cada 5 s |

Umbrales crudos (histéresis):

- `UMBRAL_RIEGO_SEC_CRUDO = 4000` → bomba ON si `ultimaHumedadValida ≥ 4000`
- `UMBRAL_RIEGO_HUM_CRUDO = 3000` → bomba OFF si ≤ 3000
- `static_assert(SEC > HUM)` en fuente.

Transición:

- `DURACION_TRANSICION_AUTO_MS = 3000`
- `autoTransicionLuces` + `parpadeoTransicionHastaMs`
- Al terminar: `ACK;IR;MODO_AUTO_LUCES_OFF`
- `fijarModoManualIR()` cancela la transición.

Teclas IR sin cambio de mapa: CH- manual, CH+ auto, 100+ alterna.

### 4. Tests

- `test_firmware_contract.py`: aserciones de
  `automatizarInvernadero`, `UMBRAL_RIEGO_SEC_CRUDO 4000`,
  `BOMBA_ON_APAGA_LUCES`, `GPIO_DIRECTO`, `DURACION_TRANSICION_AUTO_MS`.
- `test_hil_producto.py`: `BOMBA_ETAPA=GPIO_DIRECTO`.

## Sensores en vivo (COM9, sesión previa)

| Sensor | Estado |
|--------|--------|
| LDR | OK (`ADC_LDR≈1295`, `LUZ_PCT≈67–70`) |
| Tierra | OK tras agua: crudo ~1570–1722 → `HUM_PCT≈67–76` |
| DHT | Descartado (`-1`) |
| ESP32 | RAM/PSRAM sanos, WDT ON, sin brownout en la última pasada |

Calibración sigue `PROVISIONAL` (`SECO=2800`, `HUMEDO=1200`); el % es
aproximado hasta `CAL_SECO`/`CAL_HUMEDO` + `CAL_GUARDAR` con paro.

## Obsidian

- Esta nota (81) documenta la sesión.
- 76 sigue siendo el mapa IR de referencia (audio/LCD obsoletos en esa
  nota; el mapa vivo es el del firmware + 80).
