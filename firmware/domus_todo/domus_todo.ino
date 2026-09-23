/*
  ============================================================================
  PROJECT DOMUS — PROGRAMA TODO (banco integrado)
  ============================================================================
  Placa: ESP32-S3 N16R8 (COM9). Monitor Serial 115200 con NL.
  FQBN: esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB

  TODO EN UN SOLO SKETCH:
    Sensores : suelo GPIO15, nivel GPIO16, LDR GPIO3, DHT11 GPIO14 (PIR ignorado)
    Motores  : DRV8833 en modo 1-pin (AIN2 y BIN2 amarrados a GND en hardware)
               bomba = GPIO4 -> AIN1, ventilador = GPIO7 -> BIN1
               nSLEEP -> VCC, VM -> 5V bombas (fusible 2A), GND comun
    Luces    : sala GPIO5, cuarto GPIO6, cultivo GPIO8 (LED + 220 ohmios a GND)
    Sonido   : DFPlayer Mini, Serial1 RX = GPIO11, TX = GPIO18, 9600 8N1
               DFP-TX -> GPIO11, DFP-RX <- GPIO18 (serie 1k), VCC 5V, GND comun
               SD con carpetas 01-21 (Carlos) y 51-71 (Karla), pistas 001-004
    UI       : LCD 1602 SDA GPIO17/SCL GPIO13 (3V3), IR GPIO12 (21 teclas),
               PARO GPIO10 -> GND. Botones MODO/SILENCIO: DESCONECTADOS
               (sus pines 18/11 ahora son la UART del DFPlayer; el mando los
               reemplaza: CH = pagina LCD, PLAY = silencio).
    LEDS AZULES: HABLA = DFP-BUSY -> 10k -> S8050 -> LED (enciende solo
               mientras reproduce de verdad, sin gastar GPIO).
               VIVO = GPIO9 PWM -> 1k -> S8050 -> LED (respira en reposo,
               parpadea al hablar/tocar mando, parpadeo rapido en PARO).

  SEGURIDAD: bomba solo sumergida, corte a 120 s, interlock de nivel,
  PARO fisico y por Serial. Bomba y ventilador arrancan APAGADOS.
  ============================================================================
*/
#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>

// ---------- Pines ----------
#define PIN_SUELO  15
#define PIN_NIVEL  16
#define PIN_LDR    3
#define PIN_DHT    14
#define PIN_VIVO   9    // LED azul VIVO (PWM via S8050). PIR ignorado
#define PIN_PARO   10
#define PIN_IR     12
#define PIN_SCL    13
#define PIN_SDA    17
#define PIN_BOMBA  4    // DRV AIN1 (AIN2 amarrado a GND)
#define PIN_VENT   7    // DRV BIN1 (BIN2 amarrado a GND)
#define PIN_SALA   5
#define PIN_CUARTO 6
#define PIN_CULT   8
#define PIN_DFP_RX 11   // Serial1 RX <- DFP-TX
#define PIN_DFP_TX 18   // Serial1 TX -> DFP-RX
#define DHT_TIPO   DHT11

// ---------- Calibracion ----------
int calSeco = 2800, calHumedo = 1200;      // suelo: ADC seco 0%, humedo 100%
int calOscuro = 3200, calClaro = 400;      // LDR: ADC oscuro 0%, claro 100%
int calNivelVacio = 600, calNivelLleno = 2500;  // nivel: ADC vacio 0%, lleno 100%
#define UMB_RIEGO_SECO   35
#define UMB_RIEGO_HUMEDO 45
#define UMB_LUZ_OSCURO   25
#define UMB_LUZ_CLARO    40
#define UMB_TEMP_ALTA    28.0
#define UMB_TEMP_NORMAL  26.0
#define T_MAX_BOMBA_MS   120000UL
#define PIR_RET_MS       30000UL

// ---------- Teclas reales CAR MP3 (nota 64) ----------
struct TeclaIR { const char* nombre; uint16_t codigo; };
const TeclaIR TECLAS[21] = {
  {"CH-",0x45},{"CH",0x46},{"CH+",0x47},{"ANTERIOR",0x44},{"PLAY",0x43},
  {"SIGUIENTE",0x40},{"VOL-",0x07},{"VOL+",0x15},{"EQ",0x09},{"0",0x16},
  {"100+",0x19},{"200+",0x0D},{"1",0x0C},{"2",0x18},{"3",0x5E},{"4",0x08},
  {"5",0x1C},{"6",0x5A},{"7",0x42},{"8",0x52},{"9",0x4A}
};
enum { K_CH_MENOS, K_CH, K_CH_MAS, K_ANTERIOR, K_PLAY, K_SIGUIENTE,
  K_VOL_MENOS, K_VOL_MAS, K_EQ, K_0, K_100, K_200,
  K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9 };

// ---------- Estado ----------
DHT dht(PIN_DHT, DHT_TIPO);
HardwareSerial SerialDFP(1);
LiquidCrystal_I2C* lcd = nullptr;
bool bomba = false, vent = false, sala = false, cuarto = false, cult = false;
bool autoRiego = false, autoVent = false, autoSala = false, autoCuarto = false, autoCult = false;
bool paro = false, silenciado = false;
uint8_t voz = 1, volumen = 18, ultimaPista[21] = {0};
unsigned long bombaDesdeMs = 0, ultimaTeleMs = 0, ultimaPantMs = 0;
unsigned long ultimoDhtMs = 0, ultimoVivoMs = 0, vivoHastaMs = 0, irBlinkHastaMs = 0;
uint8_t ultimaCarpeta = 0, ultimaPistaRep = 0;
float tempC = -99, humAire = -99;
int fallosDht = 0;
uint8_t vistaLcd = 0;
char sombraLcd[2][17];
bool sombraLcdOk = false;
int8_t grabarIdx = -1;  // reserved
String bufSer = "";
unsigned long irTotal = 0, irConocidas = 0, irDesconocidas = 0;
uint16_t irUltimoCod = 0;

// ---------- Sensores ----------
bool leerSuelo(int &crudo, int &pct) {
  long a = 0;
  for (int i = 0; i < 8; i++) { a += analogRead(PIN_SUELO); delay(4); }
  crudo = a / 8;
  if (crudo < 50 || crudo > 4045) return false;
  long p = (long)(calSeco - crudo) * 100L / (long)(calSeco - calHumedo);
  if (p < 0) p = 0; if (p > 100) p = 100;
  pct = p; return true;
}
bool leerNivel(int &crudo, int &pct) {
  long a = 0;
  for (int i = 0; i < 8; i++) { a += analogRead(PIN_NIVEL); delay(4); }
  crudo = a / 8;
  if (crudo < 16 || crudo > 4079) return false;
  long p = (long)(crudo - calNivelVacio) * 100L / (long)(calNivelLleno - calNivelVacio);
  if (p < 0) p = 0; if (p > 100) p = 100;
  pct = p; return true;
}
bool leerLuz(int &crudo, int &pct) {
  crudo = analogRead(PIN_LDR);
  if (crudo < 16 || crudo > 4079) return false;
  long p = (long)(calOscuro - crudo) * 100L / (long)(calOscuro - calClaro);
  if (p < 0) p = 0; if (p > 100) p = 100;
  pct = p; return true;
}
bool leerAmbiente() {  // cache 2.5 s, tolera NaN como el real
  if (millis() - ultimoDhtMs < 2500 && tempC > -90) return true;
  ultimoDhtMs = millis();
  float t = dht.readTemperature(), h = dht.readHumidity();
  if (isnan(t) || isnan(h) || t < 0 || t > 50 || h < 20 || h > 90) {
    if (++fallosDht >= 3) { tempC = -99; }
    return tempC > -90;
  }
  fallosDht = 0; tempC = t; humAire = h;
  return true;
}

// ---------- DFPlayer (fire-and-forget + log, como banco sin BUSY) ----------
void dfpTrama(uint8_t cmd, uint8_t p1, uint8_t p2) {
  uint8_t t[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, p1, p2, 0, 0, 0xEF};
  uint16_t s = 0;
  for (uint8_t i = 1; i <= 6; i++) s += t[i];
  uint16_t c = 0U - s;
  t[7] = c >> 8; t[8] = c & 0xFF;
  SerialDFP.write(t, 10);
}
void dfpVol(uint8_t v) { if (v > 30) v = 30; volumen = v; dfpTrama(0x06, 0, v); }
// carpeta = indice+1 (voz1: 01-21, voz2: 51-71), pista 1-4 con rotacion
void decir(int idx, uint8_t variante) {
  uint8_t carpeta = (voz == 2) ? (uint8_t)(idx + 1 + 50) : (uint8_t)(idx + 1);
  uint8_t pista = variante;
  if (pista < 1 || pista > 4) {
    pista = (uint8_t)(ultimaPista[idx] % 4 + 1);
  }
  ultimaPista[idx] = pista;
  Serial.print("DFP>carpeta "); Serial.print(carpeta);
  Serial.print(" pista "); Serial.print(pista);
  Serial.print(" ["); Serial.print(TECLAS[idx].nombre);
  if (silenciado) { Serial.println("] SILENCIADO (no suena)"); return; }
  ultimaCarpeta = carpeta; ultimaPistaRep = pista;
  vivoHastaMs = millis() + 4000;  // LED VIVO excitado mientras "habla"
  dfpTrama(0x0F, carpeta, pista);
  Serial.println("] sonando");
}

// ---------- Actuadores ----------
void setBomba(bool on, const char* por) {
  if (on && paro) { Serial.println("NACK;RIEGO_ON;paro_emergencia"); return; }
  if (on) {
    int nc, pc;
    if (!leerNivel(nc, pc) || nc < calNivelVacio) {
      Serial.println("NACK;RIEGO_ON;nivel_bajo_o_invalido"); return;
    }
    if (!bomba) bombaDesdeMs = millis();
  } else bombaDesdeMs = 0;
  digitalWrite(PIN_BOMBA, on ? HIGH : LOW);
  bomba = on;
  Serial.print(on ? "ACK;RIEGO_ON;1;" : "ACK;RIEGO_OFF;0;");
  Serial.println(por);
}
void setVent(bool on, const char* por) {
  if (on && paro) { Serial.println("NACK;VENT_ON;paro_emergencia"); return; }
  digitalWrite(PIN_VENT, on ? HIGH : LOW);
  vent = on;
  Serial.print(on ? "ACK;VENT_ON;1;" : "ACK;VENT_OFF;0;");
  Serial.println(por);
}
void setLuz(int pin, bool &est, const char* tag, bool on, const char* por) {
  if (on && paro) { Serial.print("NACK;"); Serial.print(tag); Serial.println("_ON;paro"); return; }
  digitalWrite(pin, on ? HIGH : LOW);
  est = on;
  Serial.print("ACK;"); Serial.print(tag); Serial.print(on ? "_ON;1;" : "_OFF;0;");
  Serial.println(por);
}
void todoOff(const char* por) {
  setBomba(false, por); setVent(false, por);
  setLuz(PIN_SALA, sala, "LUZ1", false, por);
  setLuz(PIN_CUARTO, cuarto, "LUZ2", false, por);
  setLuz(PIN_CULT, cult, "LUZC", false, por);
}

void diagnostico() {
  int cs, ps, cn, pn, cl, pl;
  bool os = leerSuelo(cs, ps), on = leerNivel(cn, pn), ol = leerLuz(cl, pl);
  bool oa = leerAmbiente();
  Serial.print("DIAG;SUELO="); Serial.print(os ? ps : -1);
  Serial.print(";NIVEL="); Serial.print(on ? pn : -1);
  Serial.print(";LUZ="); Serial.print(ol ? pl : -1);
  Serial.print(";TEMP="); Serial.print(oa ? tempC : -99);
  Serial.print(";BOMBA="); Serial.print(bomba);
  Serial.print(";VENT="); Serial.print(vent);
  Serial.print(";LUCES="); Serial.print(sala); Serial.print(cuarto); Serial.print(cult);
  Serial.print(";PARO="); Serial.println(paro);
}

// ---------- LCD (4 vistas, reescritura solo si cambia) ----------
// Escritura diferencial (nota 57/64): jamas clear(), solo los caracteres
// que difieren de la sombra char[2][17]. Lineas siempre de 16 exactos.
void lcdShow(const String &l0, const String &l1) {
  if (!lcd) return;
  const String lin[2] = {l0, l1};
  for (uint8_t f = 0; f < 2; f++) {
    char norm[17];
    snprintf(norm, sizeof(norm), "%-16.16s", lin[f].c_str());
    for (uint8_t c = 0; c < 16; c++) {
      if (!sombraLcdOk || sombraLcd[f][c] != norm[c]) {
        lcd->setCursor(c, f);
        lcd->print(norm[c]);
        sombraLcd[f][c] = norm[c];
      }
    }
    sombraLcd[f][16] = '\0';
  }
  sombraLcdOk = true;
}
void refrescarLcd() {
  if (!lcd) return;
  int cs, ps, cn, pn;
  bool os = leerSuelo(cs, ps), on = leerNivel(cn, pn);
  bool oa = leerAmbiente();
  char l0[17], l1[17];
  if (paro) { lcdShow("!! PARO ACTIVO", "REARMAR: tecla"); return; }
  switch (vistaLcd % 4) {
    case 0:
      if (oa) snprintf(l0, 17, "T:%.1fC H:%.0f%%", tempC, humAire);
      else snprintf(l0, 17, "T/H: ERR");
      snprintf(l1, 17, "V:%s VOL:%d", voz == 1 ? "Carlos" : "Karla", volumen);
      break;
    case 1:
      if (os) snprintf(l0, 17, "Suelo:%d%%", ps); else snprintf(l0, 17, "Suelo:ERR");
      if (on) snprintf(l1, 17, "Agua:%d%%", pn); else snprintf(l1, 17, "Agua:ERR");
      break;
    case 2:
      snprintf(l0, 17, "B:%s V:%s", bomba ? "ON" : "OFF", vent ? "ON" : "OFF");
      snprintf(l1, 17, "S:%s C:%s I:%s", sala ? "1" : "0", cuarto ? "1" : "0", cult ? "1" : "0");
      break;
    default:
      snprintf(l0, 17, "DFP %02d/%03d", ultimaCarpeta, ultimaPistaRep);
      snprintf(l1, 17, "V:%s VOL:%d", voz == 1 ? "Carlos" : "Karla", volumen);
      break;
  }
  lcdShow(String(l0), String(l1));
}

// ---------- IR igual que el real ----------
void teclaIR(int idx, uint16_t codigo) {
  Serial.print("IR;CMD=0x"); Serial.print(codigo, HEX);
  Serial.print(";TECLA="); Serial.println(TECLAS[idx].nombre);
  switch (idx) {
    case K_5: setBomba(!bomba, "IR_5"); decir(idx, bomba ? 1 : 2); break;
    case K_4: setVent(!vent, "IR_4"); decir(idx, vent ? 1 : 2); break;
    case K_1: case K_ANTERIOR:
      setLuz(PIN_SALA, sala, "LUZ1", !sala, "IR"); decir(K_1, sala ? 1 : 2); break;
    case K_2: case K_SIGUIENTE:
      setLuz(PIN_CUARTO, cuarto, "LUZ2", !cuarto, "IR"); decir(K_2, cuarto ? 1 : 2); break;
    case K_3: setLuz(PIN_CULT, cult, "LUZC", !cult, "IR"); decir(idx, cult ? 1 : 2); break;
    case K_6: leerAmbiente();
      Serial.print("TEMP;"); Serial.println(tempC > -90 ? String(tempC, 1) : "ERR");
      decir(idx, 1); break;
    case K_7: leerAmbiente();
      Serial.print("HUM_AIRE;"); Serial.println(tempC > -90 ? String(humAire, 0) : "ERR");
      decir(idx, 1); break;
    case K_8: { int c, p, n, q; leerSuelo(c, p); leerNivel(n, q);
      Serial.print("SUELO;"); Serial.print(p); Serial.print(";AGUA;"); Serial.println(q);
      decir(idx, 1); } break;
    case K_9: diagnostico(); decir(idx, 1); break;
    case K_EQ: diagnostico(); decir(idx, 1); break;
    case K_0: todoOff("IR_0"); autoRiego = autoVent = autoSala = autoCuarto = autoCult = false;
      decir(idx, 1); break;
    case K_CH_MENOS: autoRiego = autoVent = autoSala = autoCuarto = autoCult = false;
      Serial.println("ACK;IR;MODO_MANUAL"); decir(idx, 1); break;
    case K_CH_MAS: autoRiego = autoVent = autoSala = autoCuarto = autoCult = true;
      Serial.println("ACK;IR;MODO_AUTO"); decir(idx, 1); break;
    case K_CH: vistaLcd++; Serial.println("ACK;IR;PAGINA_LCD"); decir(idx, 1); break;
    case K_PLAY: silenciado = !silenciado;
      Serial.println(silenciado ? "ACK;IR;SILENCIO_ON" : "ACK;IR;SILENCIO_OFF");
      if (!silenciado) decir(idx, 1); break;
    case K_VOL_MENOS: dfpVol(volumen - 2);
      Serial.print("ACK;IR;VOL="); Serial.println(volumen); break;
    case K_VOL_MAS: dfpVol(volumen + 2);
      Serial.print("ACK;IR;VOL="); Serial.println(volumen); break;
    case K_100: voz = (voz == 1) ? 2 : 1;
      Serial.print("ACK;IR;VOZ="); Serial.println(voz); decir(idx, voz); break;
    case K_200:
      if (digitalRead(PIN_PARO) == LOW) { Serial.println("NACK;REARMAR;boton_pulsado"); }
      else { paro = false; Serial.println("ACK;REARMAR;SEGURO"); }
      decir(idx, paro ? 4 : 1); break;
  }
}
void revisarIR() {
  if (!IrReceiver.decode()) return;
  uint16_t codigo = IrReceiver.decodedIRData.command;
  bool rep = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
  IrReceiver.resume();
  if (codigo != 0) { irTotal++; irUltimoCod = codigo; irBlinkHastaMs = millis() + 900; }
  int idx = -1;
  for (int i = 0; i < 21; i++) if (TECLAS[i].codigo == codigo) { idx = i; break; }
  if (idx < 0) {
    if (codigo == 0) return;  // ruido de pin flotante sin receptor: silencio total
    irDesconocidas++;
    static unsigned long ultimoNack = 0;
    if (millis() - ultimoNack < 2000) return;  // desconocidas: max 1 cada 2 s
    ultimoNack = millis();
    Serial.print("NACK;IR;desconocida_0x"); Serial.println(codigo, HEX); return;
  }
  if (rep && idx != K_VOL_MENOS && idx != K_VOL_MAS) return;
  irConocidas++;
  if (millis() + 1500 > vivoHastaMs) vivoHastaMs = millis() + 1500;  // reacciona al mando
  teclaIR(idx, codigo);
}

// ---------- Serial ----------
void procesar(const String &c) {
  if (c == "AYUDA") { Serial.println("ESTADO DIAGNOSTICO SUELO NIVEL RIEGO_ON/OFF/AUTO/MAN VENT_ON/OFF/AUTO/MAN LUZ1_ON/OFF/AUTO LUZ2_ON/OFF/AUTO LUZC_ON/OFF/AUTO CAL_SECO CAL_HUMEDO CAL_NIVEL TODO_OFF PARO REARMAR SONAR VOL"); }
  else if (c == "ESTADO" || c == "DIAGNOSTICO") diagnostico();
  else if (c == "SUELO") { int r, p; if (leerSuelo(r, p)) {
      Serial.print("SUELO;RAW="); Serial.print(r); Serial.print(";PCT="); Serial.println(p);
    } else Serial.println("SUELO;ERR fuera de rango"); }
  else if (c == "NIVEL") { int r, p; if (leerNivel(r, p)) {
      Serial.print("NIVEL;RAW="); Serial.print(r); Serial.print(";PCT="); Serial.println(p);
    } else Serial.println("NIVEL;ERR fuera de rango"); }
  else if (c == "RIEGO_ON") { autoRiego = false; setBomba(true, "SERIAL"); }
  else if (c == "RIEGO_OFF") { autoRiego = false; setBomba(false, "SERIAL"); }
  else if (c == "RIEGO_AUTO") { autoRiego = true; Serial.println("ACK;RIEGO_AUTO;1"); }
  else if (c == "RIEGO_MAN") { autoRiego = false; Serial.println("ACK;RIEGO_MAN;1"); }
  else if (c == "VENT_ON") { autoVent = false; setVent(true, "SERIAL"); }
  else if (c == "VENT_OFF") { autoVent = false; setVent(false, "SERIAL"); }
  else if (c == "VENT_AUTO") { autoVent = true; Serial.println("ACK;VENT_AUTO;1"); }
  else if (c == "VENT_MAN") { autoVent = false; Serial.println("ACK;VENT_MAN;1"); }
  else if (c == "LUZ1_ON") setLuz(PIN_SALA, sala, "LUZ1", true, "SERIAL");
  else if (c == "LUZ1_OFF") setLuz(PIN_SALA, sala, "LUZ1", false, "SERIAL");
  else if (c == "LUZ1_AUTO") { autoSala = true; Serial.println("ACK;LUZ1_AUTO;1"); }
  else if (c == "LUZ2_ON") setLuz(PIN_CUARTO, cuarto, "LUZ2", true, "SERIAL");
  else if (c == "LUZ2_OFF") setLuz(PIN_CUARTO, cuarto, "LUZ2", false, "SERIAL");
  else if (c == "LUZ2_AUTO") { autoCuarto = true; Serial.println("ACK;LUZ2_AUTO;1"); }
  else if (c == "LUZC_ON") setLuz(PIN_CULT, cult, "LUZC", true, "SERIAL");
  else if (c == "LUZC_OFF") setLuz(PIN_CULT, cult, "LUZC", false, "SERIAL");
  else if (c == "LUZC_AUTO") { autoCult = true; Serial.println("ACK;LUZC_AUTO;1"); }
  else if (c == "CAL_SECO") { int r, p; leerSuelo(r, p); calSeco = r;
    Serial.print("ACK;CAL_SECO;"); Serial.println(r); }
  else if (c == "CAL_HUMEDO") { int r, p; leerSuelo(r, p); calHumedo = r;
    Serial.print("ACK;CAL_HUMEDO;"); Serial.println(r); }
  else if (c == "CAL_NIVEL") { int r, p; leerNivel(r, p); calNivelVacio = r;
    Serial.print("ACK;CAL_NIVEL;"); Serial.println(r); }
  else if (c == "TODO_OFF") { todoOff("SERIAL");
    autoRiego = autoVent = autoSala = autoCuarto = autoCult = false; }
  else if (c == "PARO") { paro = true; todoOff("PARO"); Serial.println("EVENTO;PARO;ACTIVO"); }
  else if (c == "REARMAR") { paro = false; Serial.println("ACK;REARMAR;SEGURO"); }
  else if (c == "SONAR") { decir(K_9, 1); }
  else if (c == "IR_ESTADO") {
    Serial.print("IR_EST;total="); Serial.print(irTotal);
    Serial.print(";conocidas="); Serial.print(irConocidas);
    Serial.print(";desconocidas="); Serial.print(irDesconocidas);
    Serial.print(";ultimo=0x"); Serial.println(irUltimoCod, HEX);
  }
  else if (c == "VOL") { Serial.print("VOL;"); Serial.println(volumen); }
  else { Serial.print("NACK;"); Serial.print(c); Serial.println(";no_reconocido"); }
}


// ---------- LED VIVO (respira en reposo, se excita al hablar/tocar) ----------
void actualizarVivo() {
  if (millis() - ultimoVivoMs < 20) return;
  ultimoVivoMs = millis();
  int d;
  if (paro) d = ((millis() / 100) % 2) ? 255 : 0;          // PARO: parpadeo
  else if ((long)(irBlinkHastaMs - millis()) > 0)
    d = ((millis() / 150) % 2) ? 255 : 0;                  // tecla IR: triple parpadeo
  else if ((long)(vivoHastaMs - millis()) > 0)
    d = ((millis() / 60) % 2) ? random(140, 255) : random(0, 60);  // excitado
  else d = (int)(100 + 90 * sin((millis() % 3000) / 3000.0 * 6.2832));  // respira
  ledcWrite(PIN_VIVO, d);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("== DOMUS TODO v1 ==");
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(PIN_BOMBA, OUTPUT); pinMode(PIN_VENT, OUTPUT);
  pinMode(PIN_SALA, OUTPUT); pinMode(PIN_CUARTO, OUTPUT); pinMode(PIN_CULT, OUTPUT);
  digitalWrite(PIN_BOMBA, LOW); digitalWrite(PIN_VENT, LOW);
  digitalWrite(PIN_SALA, LOW); digitalWrite(PIN_CUARTO, LOW); digitalWrite(PIN_CULT, LOW);
  pinMode(PIN_PARO, INPUT_PULLUP);
  ledcAttach(PIN_VIVO, 1000, 8);  // LED azul VIVO por PWM (via S8050 a 5V)
  ledcWrite(PIN_VIVO, 0);
  dht.begin();
  IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);
  SerialDFP.begin(9600, SERIAL_8N1, PIN_DFP_RX, PIN_DFP_TX);
  delay(400);
  dfpVol(volumen);
  Serial.println("DFP iniciado RX11/TX18 (sin BUSY: log por Serial)");
  decir(K_9, 3);  // bienvenida: "Sistemas en linea" al arrancar
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
  } else Serial.println("LCD ausente: headless");
  Serial.println("AYUDA para comandos. Bomba solo sumergida.");
}

void loop() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { bufSer.trim(); bufSer.toUpperCase();
      if (bufSer.length()) procesar(bufSer); bufSer = ""; }
    else if (bufSer.length() < 20) bufSer += ch;
  }
  revisarIR();
  actualizarVivo();
  if (digitalRead(PIN_PARO) == LOW && !paro) {
    paro = true; todoOff("PARO_FISICO"); Serial.println("EVENTO;PARO;ACTIVO");
  }
  if (bomba && millis() - bombaDesdeMs > T_MAX_BOMBA_MS) {
    setBomba(false, "CORTE_120S"); Serial.println("EVENTO;BOMBA;CORTE_120S");
  }
  // ---- automaticos ----
  if (!paro) {
    int v, pv;
    if (autoRiego && leerSuelo(v, pv)) {
      if (!bomba && pv <= UMB_RIEGO_SECO) { setBomba(true, "AUTO"); decir(K_5, 3); }
      else if (bomba && pv >= UMB_RIEGO_HUMEDO) { setBomba(false, "AUTO"); decir(K_5, 4); }
    }
    if (autoVent && leerAmbiente()) {
      if (!vent && tempC >= UMB_TEMP_ALTA) { setVent(true, "AUTO"); }
      else if (vent && tempC <= UMB_TEMP_NORMAL) { setVent(false, "AUTO"); }
    }
    int cl, pl;
    bool presRec = true;  // PIR ignorado (GPIO9 es LED VIVO): presencia asumida
    if (leerLuz(cl, pl)) {
      if (autoSala) {
        if (!sala && pl <= UMB_LUZ_OSCURO && presRec) setLuz(PIN_SALA, sala, "LUZ1", true, "AUTO");
        else if (sala && (pl >= UMB_LUZ_CLARO || !presRec)) setLuz(PIN_SALA, sala, "LUZ1", false, "AUTO");
      }
      if (autoCuarto) {
        if (!cuarto && pl <= UMB_LUZ_OSCURO && presRec) setLuz(PIN_CUARTO, cuarto, "LUZ2", true, "AUTO");
        else if (cuarto && (pl >= UMB_LUZ_CLARO || !presRec)) setLuz(PIN_CUARTO, cuarto, "LUZ2", false, "AUTO");
      }
      if (autoCult) {
        if (!cult && pl <= UMB_LUZ_OSCURO) setLuz(PIN_CULT, cult, "LUZC", true, "AUTO");
        else if (cult && pl >= UMB_LUZ_CLARO) setLuz(PIN_CULT, cult, "LUZC", false, "AUTO");
      }
    }
  }
  if (millis() - ultimaPantMs > 1500) { ultimaPantMs = millis(); refrescarLcd(); }
  if (millis() - ultimaTeleMs > 2000) {
    ultimaTeleMs = millis();
    int cs, ps, cn, pn;
    bool os = leerSuelo(cs, ps), on = leerNivel(cn, pn);
    Serial.print("TODO;SUELO="); Serial.print(os ? ps : -1);
    Serial.print(";AGUA="); Serial.print(on ? pn : -1);
    Serial.print(";B="); Serial.print(bomba); Serial.print(";V="); Serial.print(vent);
    Serial.print(";S="); Serial.print(sala); Serial.print(";C="); Serial.print(cuarto);
    Serial.print(";I="); Serial.println(cult);
  }
}
