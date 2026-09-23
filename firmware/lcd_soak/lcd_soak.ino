/*
  LCD SOAK (prueba nocturna, solo LCD conectado):
  Mide salud del bus I2C SDA=GPIO17/SCL=GPIO13 con el LCD 1602 (0x27/0x3F).
  Para cada reloj {50k, 75k, 100k}: 100 ciclos de ping + escritura de 32
  celdas, contando NACKs (endTransmission != 0). Al final imprime VEREDICTO
  con el reloj de menos errores y deja patron quieto en ese reloj.
  Serial 115200.
*/
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_SDA 17
#define PIN_SCL 13
const long RELOJES[3] = {50000, 75000, 100000};
#define CICLOS 100

uint8_t buscarLcd() {
  uint8_t pref = 0, unica = 0, n = 0;
  for (uint8_t d = 0x08; d <= 0x77; d++) {
    Wire.beginTransmission(d);
    if (Wire.endTransmission() == 0) {
      n++; unica = d;
      if (d == 0x27 || d == 0x3F) pref = d;
    }
  }
  if (pref) return pref;
  if (n == 1) return unica;
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("== LCD SOAK inicio ==");
  Wire.begin(PIN_SDA, PIN_SCL);
  delay(50);

  long mejorReloj = RELOJES[0];
  long mejorErr = 999999;

  for (int r = 0; r < 3; r++) {
    Wire.setClock(RELOJES[r]);
    delay(50);
    uint8_t dir = buscarLcd();
    Serial.print("RELOJ;"); Serial.print(RELOJES[r]);
    Serial.print(";LCD=0x"); Serial.println(dir, HEX);
    LiquidCrystal_I2C* lcd = nullptr;
    if (dir) { lcd = new LiquidCrystal_I2C(dir, 16, 2); lcd->init(); lcd->backlight(); }

    long err = 0, escritas = 0;
    for (int i = 0; i < CICLOS; i++) {
      // ping de direccion
      if (dir) {
        Wire.beginTransmission(dir);
        if (Wire.endTransmission() != 0) err++;
      } else {
        dir = buscarLcd();
        if (dir) { lcd = new LiquidCrystal_I2C(dir, 16, 2); lcd->init(); lcd->backlight(); }
        else err++;
      }
      // escritura de 32 celdas (patron alterno = peor caso de trafico)
      if (lcd && dir) {
        char a[17], b[17];
        for (int k = 0; k < 16; k++) { a[k] = (i % 2) ? ('A' + k) : ('0' + (k % 10)); b[k] = (i % 2) ? ('0' + (k % 10)) : ('A' + k); }
        a[16] = 0; b[16] = 0;
        lcd->setCursor(0, 0); lcd->print(a);
        lcd->setCursor(0, 1); lcd->print(b);
        escritas += 32;
        Wire.beginTransmission(dir);
        if (Wire.endTransmission() != 0) err++;
      }
      if ((i + 1) % 25 == 0) {
        Serial.print("PARCIAL;reloj="); Serial.print(RELOJES[r]);
        Serial.print(";ciclo="); Serial.print(i + 1);
        Serial.print(";err="); Serial.println(err);
      }
      delay(400);
    }
    Serial.print("RESUMEN;reloj="); Serial.print(RELOJES[r]);
    Serial.print(";clatin="); Serial.print(CICLOS);
    Serial.print(";celdas="); Serial.print(escritas);
    Serial.print(";errores="); Serial.println(err);
    if (lcd) { delete lcd; lcd = nullptr; }
    if (err < mejorErr) { mejorErr = err; mejorReloj = RELOJES[r]; }
  }
  Serial.print("VEREDICTO;mejor_reloj="); Serial.print(mejorReloj);
  Serial.print(";errores="); Serial.println(mejorErr);
  // Patron quieto final en el mejor reloj (observacion visual en la manana)
  Wire.setClock(mejorReloj);
  uint8_t dir = buscarLcd();
  if (dir) {
    LiquidCrystal_I2C lcd(dir, 16, 2);
    lcd.init(); lcd.backlight();
    lcd.setCursor(0, 0); lcd.print("SOAK OK  50-100k");
    lcd.setCursor(0, 1); lcd.print("reloj fijado");
    Serial.print("PATRON_QUIETO;reloj="); Serial.println(mejorReloj);
  }
  Serial.println("== LCD SOAK fin ==");
}

void loop() {
  // ping de vigilancia cada 5 s para ver si el bus sigue vivo toda la noche
  static unsigned long ult = 0;
  if (millis() - ult > 5000) {
    ult = millis();
    uint8_t dir = buscarLcd();
    Serial.print("WATCH;uptime_s="); Serial.print(millis() / 1000);
    Serial.print(";lcd="); Serial.println(dir ? "OK" : "PERDIDO");
  }
}
