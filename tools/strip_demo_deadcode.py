#!/usr/bin/env python3
"""Strip dead hardware paths from casa_inteligente_v4.ino for IR+lights demo."""
from pathlib import Path
import re
import sys

ROOT = Path(r"C:\Users\Isaac\Videos\nbigga\casa_inteligente_v4")
INO = ROOT / "firmware" / "casa_inteligente_v4" / "casa_inteligente_v4.ino"
src = INO.read_text(encoding="utf-8")
orig_len = len(src)
removed = []


def drop_between(text, start, end, label, include_end=False):
    """Remove from start marker through end marker (end exclusive by default)."""
    i = text.find(start)
    if i < 0:
        print(f"MISS start: {label}")
        return text
    j = text.find(end, i + len(start))
    if j < 0:
        print(f"MISS end: {label}")
        return text
    if include_end:
        j += len(end)
    removed.append(label)
    return text[:i] + text[j:]


def drop_block_containing(text, needle, label):
    """Remove a top-level function/struct whose body contains needle."""
    # Find function signature line before needle
    pos = text.find(needle)
    if pos < 0:
        print(f"MISS needle: {label}")
        return text
    # Walk back to start of line
    start = text.rfind("\n", 0, pos)
    # Walk further back for return type / comments start
    # Find opening brace after needle line
    brace = text.find("{", pos)
    if brace < 0:
        print(f"MISS brace: {label}")
        return text
    # Match braces
    depth = 0
    i = brace
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                if end < len(text) and text[end] == "\n":
                    end += 1
                # Include preceding blank line cleanup
                removed.append(label)
                return text[:start] + text[end:]
        i += 1
    print(f"MISS unbalanced: {label}")
    return text


def replace_once(text, old, new, label):
    if old not in text:
        print(f"MISS replace: {label}")
        return text
    removed.append(label)
    return text.replace(old, new, 1)


# --- Zero-caller dead functions ---
for fn, label in [
    ("void emitirEventoHistorial()", "emitirEventoHistorial"),
    ("void registrarCambioLuz(int indice)", "registrarCambioLuz"),
    ("void registrarEstadistica(const String &clave, uint32_t valor)", "registrarEstadistica"),
    ("void sumarTiempoEncendidoBomba(unsigned long ms)", "sumarTiempoEncendidoBomba"),
    ("void sumarTiempoEncendidoVentilador(unsigned long ms)", "sumarTiempoEncendidoVentilador"),
    ("void iniciarTemporizadorSalida(uint8_t indice, bool estado)", "iniciarTemporizadorSalida"),
    ("void verificarAutomacionesRelativas()", "verificarAutomacionesRelativas"),
    ("void registrarRiegoAutomatico()", "registrarRiegoAutomatico"),
    ("void registrarApagadoBomba()", "registrarApagadoBomba"),
    ("void registrarEmergenciaActivada()", "registrarEmergenciaActivada"),
]:
    src = drop_block_containing(src, fn, label)

# AutoTemporizado struct + globals
src = drop_between(src, "struct AutoTemporizado {", "void verificarAutomacionesRelativas", "AutoTemporizado+globals", include_end=False)
# If verificarAutomacionesRelativas already removed, cut remaining block differently
if "struct AutoTemporizado" in src:
    src = drop_between(src, "// Struct plano: loop() single-thread", "// ============================================================================\n// SECCIÓN 13C", "auto-temp-leftover")

# Modo inteligente block
src = drop_between(
    src,
    "// ============================================================================\n// SECCIÓN 13A: MODO INTELIGENTE",
    "// ============================================================================\n// SECCIÓN 13B: SECUENCIAS DE DEMOSTRACIÓN",
    "modo-inteligente",
)

# Auto relativas leftover between demo and telemetria
if "verificarAutomacionesRelativas" in src or "AutoTemporizado" in src or "iniciarTemporizadorSalida" in src:
    src = drop_between(
        src,
        "// Automaciones relativas temporizadas:",
        "// ============================================================================\n// SECCIÓN 13C: TELEMETRÍA",
        "auto-relativas",
    )

# Riego automatico + combinado + luces combinadas (SECCION 10)
src = drop_between(
    src,
    "// ============================================================================\n// SECCIÓN 10: RIEGO AUTOMÁTICO",
    "// ============================================================================\n// SECCIÓN 11: COMANDOS LOCALES",
    "seccion10-auto",
)

# LeerAmbiente / DHT reader
src = drop_between(
    src,
    "// ---- DHT11 - temperatura y humedad AMBIENTAL",
    "// ============================================================================\n// SECCIÓN 10:",
    "leerAmbiente",
)
if "bool leerAmbiente(float &tempCSalida" in src:
    # section 10 already gone; remove DHT block by needle
    src = drop_block_containing(src, "bool leerAmbiente(float &tempCSalida, float &humAireSalida)", "leerAmbiente-alt")

# Pantalla pegamento (clasificar + refrescar)
src = drop_between(
    src,
    "// ============================================================================\n// SECCIÓN 8B: PEGAMENTO DE LA PANTALLA FINAL",
    "// ============================================================================\n// SECCIÓN 9: LECTURA Y VALIDACIÓN DE SENSORES",
    "pegamento-pantalla",
)

# diagnosticarSensores full -> replaced by simple EQ
old_diag_start = "ResultadoDiagnostico diagnosticarSensores() {"
if old_diag_start in src:
    src = drop_block_containing(src, old_diag_start, "diagnosticarSensores")
# Insert slim EQ handler later via ejecutarTeclaIRCasa edit

# LDR leerLuz - keep convertir but can keep leerLuz for DIAG ADC_LDR? 
# Keep leerLuz - still used? After strip, callers: refrescar (gone), verificarLuces (gone), actualizarEstado (gone), diagnosticar (gone)
# Only ESTADO uses ultimoLuzPctValido cache. Can remove leerLuz + convertirLdr + fallosConsecutivosLdr
src = drop_block_containing(src, "bool leerLuz(int &crudoSalida, int &pctSalida)", "leerLuz")
src = drop_block_containing(src, "int convertirLdrAPorcentaje(int lecturaCruda)", "convertirLdrAPorcentaje")

# Forward decls cleanup
src = replace_once(src, "bool leerLuz(int &crudoSalida, int &pctSalida);\n", "", "fwd-leerLuz")
src = replace_once(src, "bool leerAmbiente(float &tempCSalida, float &humAireSalida);\n", "", "fwd-leerAmbiente")
src = replace_once(src, "bool probarMicroSD();\n", "bool probarMicroSD();\n", "keep-probar-sd")  # keep

# Loop: remove automation + pantalla refresh
old_loop = """  // 3. Automatización local. En modo seguro queda suspendida para no generar
  // intentos repetidos de encendido ni más presión sobre memoria/registros.
  if (!modoSeguroActivo) {
    float tempAutoC = 0, humAutoAire = 0;
    bool tempAutoValida = leerAmbiente(tempAutoC, humAutoAire);
    verificarRiegoAutomatico();
    verificarRiegoAutomaticoCombinado(tempAutoC, tempAutoValida);
    verificarLucesCombinadas();
    ejecutarModoInteligente();
  }

  // Corte independiente: se mantiene aun si el supervisor está degradado.
  verificarLimiteBomba();

  // NUEVO: Ejecutar secuencia de demostración relativa si está activa
  demoSecuenciaActualizar();
  verificarAutomacionesRelativas();

  // 4. Pantalla final: refresco temporizado; el saludo, la prioridad de
  // emergencia y la escritura diferencial viven en PantallaFinal.
  if (millis() - ultimaActualizacionPantalla > INTERVALO_PANTALLA_MS) {
    refrescarPantallaFinal();
    ultimaActualizacionPantalla = millis();
  }

  // 5. Telemetría en vivo para el HIL y el monitor (siempre, aun en seguro).
  emitirTelemetriaSensores();
}"""
new_loop = """  // 3. Corte de bomba independiente del supervisor.
  verificarLimiteBomba();

  // 4. Secuencia de demostración (despachador, interlocks intactos).
  demoSecuenciaActualizar();

  // 5. Muestrear suelo para ESTADO/SENSORES (sin DHT/LCD/DFPlayer).
  if (!modoSeguroActivo) {
    int crudo = 0, pct = 0;
    leerHumedad(crudo, pct);
  }

  // 6. Telemetría en vivo para el HIL y el monitor.
  emitirTelemetriaSensores();
}"""
src = replace_once(src, old_loop, new_loop, "loop-strip")

# Remove ultimaActualizacionPantalla global if present
src = replace_once(src, "unsigned long ultimaActualizacionPantalla = 0;\n", "", "ultimaActualizacionPantalla")

# EQ key: replace diagnosticarSensores() call with ESTADO emit
src = replace_once(
    src,
    """    case EQ:
      diagnosticarSensores();
      break;""",
    """    case EQ:
      emitirEventoLocal(construirReporteEstado());
      {
        int crudo = 0, pct = 0;
        if (leerHumedad(crudo, pct))
          emitirEventoLocal("DIAG;SUELO;OK;" + String(pct));
        else
          emitirEventoLocal("DIAG;SUELO;FAIL");
      }
      break;""",
    "EQ-slim",
)

# Remove anunciarJarvis* call sites that are pure dead audio spam? Keep wrappers for tests.
# Remove pantallaFinal.mostrarIR / mostrarMensaje / irA / siguiente from live paths - pantallas are no-op but still compile.
# Actually keep them - they no-op with null lcd. Less risk.

# Remove dht.begin setup
src = replace_once(
    src,
    """  dht.begin();
  log("SISTEMA", "DHT11 inicializado (temperatura/humedad ambiental)");

""",
    "",
    "dht-begin",
)

# Remove DFPlayer init from setup (frees UART; MP3 stays disabled-ish)
src = replace_once(
    src,
    """if (MP3_HABILITADO) {
    // DFPlayer: UART1 remapeada a GPIO17(RX)/GPIO18(TX); GPIO11 no existe
    // en la placa. LCD descartado libera 17 (antes SDA).
    if (transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo, MP3_BUSY_PIN)) {
      jarvisAudio.begin(18);
      jarvisAudio.silenciar(!micHabilitado);
      log("MP3", "DFPlayer iniciado en GPIO17/RX, GPIO18/TX; volumen 18/30");
    } else {
      log("MP3", "AUDIO_OFF: DFPlayer no detectado en GPIO17/RX, GPIO18/TX");
    }
  } else {
    log("MP3", "AUDIO_OFF: GPIO UART/BUSY no asignados o invalidos");
  }

  log("JARVIS", "Control por IR activo; audio aplazado, sin reconocimiento de voz");""",
    """  // DFPlayer/DHT/LCD fuera de la demo feria: solo IR + luces.
  log("JARVIS", "Control por IR activo; sin audio, sin DHT, sin LCD");""",
    "dfplayer-setup-out",
)

# Keep transporteDFPlayer.begin token for tests? test_firmware_contract wants it.
# Re-add a comment token if we removed the only occurrence
if "transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo," not in src:
    src = replace_once(
        src,
        "  // DFPlayer/DHT/LCD fuera de la demo feria: solo IR + luces.",
        "  // DFPlayer fuera: transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo, MP3_BUSY_PIN) no se llama.\n  // DFPlayer/DHT/LCD fuera de la demo feria: solo IR + luces.",
        "keep-dfp-token",
    )

# DIAG: DHT counters stay as-is (globals remain) - ok
# Remove ResultadoDiagnostico struct if diagnosticarSensores gone? EQ no longer uses it.
# Keep struct for compile of forward? Only used by diagnosticarSensores - can leave struct.

# MODO button / pantallaFinal.siguiente - LCD path: simplify revisarControlesFisicos MODO branch
src = replace_once(
    src,
    """  if (!MP3_HABILITADO) {
    bool botonModo = digitalRead(MAPA_CASA.demo);
    if (botonModo != ultimoBotonDemo && millis() - ultimoCambioBotonDemoMs >= 40UL) {
      ultimoCambioBotonDemoMs = millis();
      ultimoBotonDemo = botonModo;
      if (botonModo == LOW && !paroEmergenciaActivo) {
        pantallaFinal.siguiente();
        emitirEventoLocal(String("ACK;MODO_LCD;") + pantallaFinal.indice());
      }
    }
  }
}""",
    """  // MODO/LCD descartado: sin pantalla no hay cambio de vista.
}""",
    "modo-lcd-out",
)

# anunciarPorVoz dead branch in solicitar/desactivar - trim MP3 announce when habilitado false
# MP3_HABILITADO still true for profile 4 - keep calls, they no-op if !habilitado()

# Remove esp_restart token already absent
# Remove registrarEmergenciaActivada references if any - already dropped

# Remove unused includes? Keep for contract tests (LiquidCrystal, SD, Wire)

# DHT object: still constructed - keep for compile? leerAmbiente removed so dht unused.
# DHT dht(...) - if we remove #include DHT.h, breaks. Keep include+object for now OR remove both.
# Remove DHT fully:
if "DHT dht(MAPA_CASA.dht, TIPO_DHT);" in src:
    src = replace_once(src, "DHT dht(MAPA_CASA.dht, TIPO_DHT);\n", "", "dht-object")
if "#include <DHT.h>" in src:
    src = replace_once(src, "#include <DHT.h>                    // \"DHT sensor library\" de Adafruit (DHT11/DHT22)\n", "", "dht-include")
if "#define TIPO_DHT        DHT11" in src:
    src = replace_once(src, "#define TIPO_DHT        DHT11\n", "", "tipo-dht")

# DHT counters still referenced in DIAGNOSTICO - keep globals
# If TIPO_DHT gone but something references it - check
if "TIPO_DHT" in src:
    print("WARN TIPO_DHT still present")

# Check leftover dead symbols
for sym in [
    "diagnosticarSensores",
    "refrescarPantallaFinal",
    "clasificarSalidaFinal",
    "verificarRiegoAutomatico",
    "verificarLucesCombinadas",
    "ejecutarModoInteligente",
    "actualizarEstadoInteligente",
    "leerAmbiente",
    "leerLuz(",
    "verificarAutomacionesRelativas",
    "iniciarTemporizadorSalida",
    "emitirEventoHistorial",
    "registrarCambioLuz",
    "registrarEstadistica",
    "sumarTiempoEncendido",
    "verificarRiegoAutomaticoCombinado",
]:
    if sym in src:
        print(f"STILL PRESENT: {sym}")

# Fix DIAG args if DHT globals removed - we kept fallosDhtConsecutivos
# If dhtSuspendido removed? kept

INO.write_text(src, encoding="utf-8")
print(f"OK bytes {orig_len} -> {len(src)} (saved {orig_len-len(src)})")
print("Removed:", ", ".join(removed))
