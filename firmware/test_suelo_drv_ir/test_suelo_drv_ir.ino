/*
  ============================================================================
  PROJECT DOMUS — TEST SUELO + DRV8833 + IR (banco)
  ============================================================================
  Placa: ESP32-S3 N16R8 (COM9). Monitor Serial 115200 con NL.
  FQBN: esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB

  CABLEADO (igual que perfil banco, sin 74HC595):
    Suelo:  AO -> GPIO15, VCC -> 3V3, GND -> GND (DO libre)
    DRV8833 canal A (bomba): AIN1 -> GPIO4, AIN2 -> GPIO7
    DRV8833 canal B (ventilador): BIN1 -> GPIO5, BIN2 -> GPIO6
    DRV8833: nSLEEP -> VCC, VM -> 5V bombas (fusible 2A), GND comun con ESP32
    IR VS1838B: OUT -> GPIO12, VCC -> 3V3, GND -> GND
    PARO: GPIO10 -> boton -> GND (INPUT_PULLUP)
    LCD 1602 (opcional): SDA -> GPIO17, SCL -> GPIO13, VCC -> 3V3

  REGLAS: bomba SOLO sumergida. Corte automatico a los 120 s.
  GPIO5/6 aqui son DRV (ventilador): desconecta los LED sala/cuarto del banco.
  ============================================================================
*/
#include <Arduino.h>
#include <Wire.h>
#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>

// ---- Pines (mapa real del proyecto) ----
#define PIN_SUELO   15
#define PIN_AIN1    4
#define PIN_AIN2    7
#define PIN_BIN1    5
#define PIN_BIN2    6
#define PIN_IR      12
#define PIN_PARO    10
#define PIN_SDA     17
#define PIN_SCL     13

// ---- Calibracion suelo (se ajusta con CAL_SECO / CAL_HUMEDO) ----
int sueloSeco = 2800;    // ADC en tierra seca  -> 0%
int sueloHumedo = 1200;  // ADC en tierra mojada -> 100%
#define UMBRAL_SECO_PCT   35
#define UMBRAL_HUMEDO_PCT 45
#define ADC_MIN_VALIDO    50
#define ADC_MAX_VALIDO    4045
#define TIEMPO_MAX_BOMBA_MS 120000UL

// ---- Teclas reales del mando CAR MP3 (nota Obsidian 64, protocolo P7) ----
struct TeclaIR { const char* nombre; uint16_t codigo; };
const TeclaIR TECLAS[21] = {
  {"CH-",0x45},{"CH",0x46},{"CH+",0x47},{"ANTERIOR",0x44},{"PLAY",0x43},
  {"SIGUIENTE",0x40},{"VOL-",0x07},{"VOL+",0x15},{"EQ",0x09},{"0",0x16},
  {"100+",0x19},{"200+",0x0D},{"1",0x0C},{"2",0x18},{"3",0x5E},{"4",0x08},
  {"5",0x1C},{"6",0x5A},{"7",0x42},{"8",0x52},{"9",0x4A}
};
// Indice en TECLAS de cada accion (igual que firmware real: evento = indice+1)
#define T_CH_MENOS 0
#define T_CH_MAS   2
#define T_EQ       8
#define T_0        9
#define T_4        15
#define T_5        16
#define T_8        19
#define T_9        20

bool bombaOn = false, ventOn = false, riegoAuto = false, paro = false;
unsigned long bombaDesdeMs = 0, ultimaTeleMs = 0;
LiquidCrystal_I2C* lcd = nullptr;
String ultimaLineaLcd = "";

// ---- Suelo ----
bool leerSuelo(int &crudo, int &pct) {
  long acc = 0;
  for (int i = 0; i < 8; i++) { acc += analogRead(PIN_SUELO); delay(5); }
  crudo = acc / 8;
  if (crudo < ADC_MIN_VALIDO || crudo > ADC_MAX_VALIDO) return false;
  long p = (long)(sueloSeco - crudo) * 100L / (long)(sueloSeco - sueloHumedo);
  if (p < 0) p = 0; if (p > 100) p = 100;
  pct = p;
  return true;
}

// ---- DRV8833 (adelante = IN1 HIGH + IN2 LOW, apagado = ambos LOW) ----
void bomba(bool on, const char* por) {
  if (on && paro) { Serial.println("NACK;RIEGO_ON;paro_emergencia"); return; }
  if (on && !bombaOn) bombaDesdeMs = millis();
  if (!on) bombaDesdeMs = 0;
  digitalWrite(PIN_AIN1, on ? HIGH : LOW);
  digitalWrite(PIN_AIN2, LOW);
  bombaOn = on;
  Serial.print(on ? "ACK;RIEGO_ON;1;" : "ACK;RIEGO_OFF;0;");
  Serial.println(por);
}
void vent(bool on, const char* por) {
  if (on && paro) { Serial.println("NACK;VENT_ON;paro_emergencia"); return; }
  digitalWrite(PIN_BIN1, on ? HIGH : LOW);
  digitalWrite(PIN_BIN2, LOW);
  ventOn = on;
  Serial.print(on ? "ACK;VENT_ON;1;" : "ACK;VENT_OFF;0;");
  Serial.println(por);
}
void todoOff(const char* por) { bomba(false, por); vent(false, por); }

void diagnostico() {
  int crudo, pct; bool ok = leerSuelo(crudo, pct);
  Serial.print("DIAG;SUELO_RAW="); Serial.print(ok ? crudo : -1);
  Serial.print(";SUELO_PCT="); Serial.print(ok ? pct : -1);
  Serial.print(";BOMBA="); Serial.print(bombaOn);
  Serial.print(";VENT="); Serial.print(ventOn);
  Serial.print(";MODO="); Serial.print(riegoAuto ? "AUTO" : "MAN");
  Serial.print(";PARO="); Serial.print(paro);
  Serial.print(";CAL="); Serial.print(sueloSeco);
  Serial.print("/"); Serial.println(sueloHumedo);
}

void lcdShow(const String &l0, const String &l1) {
  if (!lcd) return;
  String junto = l0 + "|" + l1;
  if (junto == ultimaLineaLcd) return;
  ultimaLineaLcd = junto;
  char a[17], b[17];
  snprintf(a, sizeof(a), "%-16.16s", l0.c_str());
  snprintf(b, sizeof(b), "%-16.16s", l1.c_str());
  lcd->setCursor(0, 0); lcd->print(a);
  lcd->setCursor(0, 1); lcd->print(b);
}

// ---- Acciones IR (mismo criterio que el firmware real) ----
void teclaIR(int idx, uint16_t codigo) {
  Serial.print("IR;CMD=0x"); Serial.print(codigo, HEX);
  Serial.print(";TECLA="); Serial.println(TECLAS[idx].nombre);
  switch (idx) {
    case T_5: bomba(!bombaOn, "IR_5"); break;
    case T_4: vent(!ventOn, "IR_4"); break;
    case T_8: { int c, p; if (leerSuelo(c, p)) {
        Serial.print("SUELO;RAW="); Serial.print(c);
        Serial.print(";PCT="); Serial.println(p);
      } else Serial.println("SUELO;ERROR Khalid fuera de rango"); } break;
    case T_9: diagnostico(); break;
    case T_EQ: diagnostico(); break;
    case T_0: todoOff("IR_0"); riegoAuto = false; break;
    case T_CH_MENOS: riegoAuto = false; Serial.println("ACK;IR;MODO_MANUAL"); break;
    case T_CH_MAS: riegoAuto = true; Serial.println("ACK;IR;MODO_AUTO"); break;
    default: Serial.println("ACK;IR;tecla_valida_sin_accion_en_test"); break;
  }
}

void revisarIR() {
  if (!IrReceiver.decode()) return;
  uint16_t codigo = IrReceiver.decodedIRData.command;
  bool rep = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
  IrReceiver.resume();
  int idx = -1;
  for (int i = 0; i < 21; i++) if (TECLAS[i].codigo == codigo) { idx = i; break; }
  if (idx < 0) { Serial.print("NACK;IR;desconocida_0x"); Serial.println(codigo, HEX); return; }
  // Igual que el real: repetida sostenida solo pasa para VOL
  if (rep && idx != 6 && idx != 7) return;
  teclaIR(idx, codigo);
}

void procesar(const String &c) {
  if (c == "AYUDA") { Serial.println("CMD: ESTADO SUELO RIEGO_ON/OFF/AUTO/MAN VENT_ON/OFF CAL_SECO CAL_HUMEDO TODO_OFF PARO REARMAR"); }
  else if (c == "ESTADO" || c == "DIAGNOSTICO") diagnostico();
  else if (c == "SUELO") { int r, p; if (leerSuelo(r, p)) {
      Serial.print("SUELO;RAW="); Serial.print(r); Serial.print(";PCT="); Serial.println(p);
    } else Serial.println("SUELO;ERROR Khalid fuera de rango (revisa VCC 3V3/GND/AO-GPIO15)"); }
  else if (c == "RIEGO_ON") bomba(true, "SERIAL");
  else if (c == "RIEGO_OFF") bomba(false, "SERIAL");
  else if (c == "RIEGO_AUTO") { riegoAuto = true; Serial.println("ACK;RIEGO_AUTO;1"); }
  else if (c == "RIEGO_MAN") { riegoAuto = false; Serial.println("ACK;RIEGO_MAN;1"); }
  else if (c == "VENT_ON") vent(true, "SERIAL");
  else if (c == "VENT_OFF") vent(false, "SERIAL");
  else if (c == "TODO_OFF") { todoOff("SERIAL"); riegoAuto = false; }
  else if (c == "CAL_SECO") { int r, p; leerSuelo(r, p); sueloSeco = r;
    Serial.print("ACK;CAL_SECO;"); Serial.println(r); }
  else if (c == "CAL_HUMEDO") { int r, p; leerSuelo(r, p); sueloHumedo = r;
    Serial.print("ACK;CAL_HUMEDO;"); Serial.println(r); }
  else if (c == "PARO") { paro = true; todoOff("PARO"); Serial.println("EVENTO;PARO;ACTIVO"); }
  else if (c == "REARMAR") { paro = false; Serial.println("ACK;REARMAR;SEGURO"); }
  else { Serial.print("NACK;"); Serial.print(c); Serial.println(";no_reconocido"); }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("== DOMUS TEST SUELO+DRV8833+IR ==");
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(PIN_AIN1, OUTPUT); pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT); pinMode(PIN_BIN2, OUTPUT);
  digitalWrite(PIN_AIN1, LOW); digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW); digitalWrite(PIN_BIN2, LOW);
  pinMode(PIN_PARO, INPUT_PULLUP);
  IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);
  Serial.println("IR listo GPIO12 (21 teclas CAR MP3, tabla fija nota 64)");
  // LCD opcional: mismo bus que el banco (SDA17/SCL13 a 50 kHz)
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(50000);
  delay(50);
  uint8_t dir = 0;
  for (uint8_t d = 0x08; d <= 0x77 && !dir; d++) {
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0 && (d == 0x27 || d == 0x3F)) dir = d;
  }
  if (dir) {
    lcd = new LiquidCrystal_I2C(dir, 16, 2);
    lcd->init(); lcd->backlight();
    Serial.print("LCD en 0x"); Serial.println(dir, HEX);
  } else Serial.println("LCD ausente: modo headless (normal sin pantalla)");
  Serial.println("Escribe AYUDA. Bomba solo sumergida.");
}

String buf = "";
void loop() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { buf.trim(); buf.toUpperCase(); if (buf.length()) procesar(buf); buf = ""; }
    else if (buf.length() < 20) buf += ch;
  }
  if (digitalRead(PIN_PARO) == LOW && !paro) {
    paro = true; todoOff("PARO_FISICO"); Serial.println("EVENTO;PARO;ACTIVO");
  }
  // Corte de seguridad bomba 120 s
  if (bombaOn && millis() - bombaDesdeMs > TIEMPO_MAX_BOMBA_MS) {
    bomba(false, "CORTE_120S"); Serial.println("EVENTO;BOMBA;CORTE_SEGURIDAD_120S");
  }
  // Riego automatico por suelo
  if (riegoAuto && !paro) {
    int r, p;
    if (leerSuelo(r, p)) {
      if (!bombaOn && p <= UMBRAL_SECO_PCT) bomba(true, "AUTO_SECO");
      else if (bombaOn && p >= UMBRAL_HUMEDO_PCT) bomba(false, "AUTO_HUMEDO");
    }
  }
  // Telemetria + LCD cada 2 s
  if (millis() - ultimaTeleMs > 2000) {
    ultimaTeleMs = millis();
    int r, p; bool ok = leerSuelo(r, p);
    Serial.print("SUELO;RAW="); Serial.print(ok ? r : -1);
    Serial.print(";PCT="); Serial.print(ok ? p : -1);
    Serial.print(";BOMBA="); Serial.print(bombaOn);
    Serial.print(";VENT="); Serial.println(ventOn);
    if (lcd) {
      char l0[17], l1[17];
      if (ok) snprintf(l0, sizeof(l0), "Suelo %d%%", p);
      else snprintf(l0, sizeof(l0), "Suelo ERR");
      snprintf(l1, sizeof(l1), "B:%s V:%s %s", bombaOn ? "ON" : "OFF",
               ventOn ? "ON" : "OFF", riegoAuto ? "AU" : "MA");
      lcdShow(String(l0), String(l1));
    }
  }
}
