#pragma once
#include <Arduino.h>

// Transporte mínimo del protocolo serie del DFPlayer Mini. No asigna GPIO:
// RX/TX/BUSY pertenecen al futuro perfil CASA_FINAL_DRV8833_DFPLAYER.
class DFPlayerTransport {
 public:
  explicit DFPlayerTransport(HardwareSerial& serial) : serial_(serial) {}

  bool begin(int8_t rx, int8_t tx, int8_t busy = -1) {
    if (rx < 0 || tx < 0 || rx == tx) return false;
    busy_ = busy;
    if (busy_ >= 0) pinMode(busy_, INPUT_PULLUP);
    serial_.begin(9600, SERIAL_8N1, rx, tx);
    listo_ = true;
    return true;
  }

  bool listo() const { return listo_; }
  bool ocupado() const { return listo_ && busy_ >= 0 && digitalRead(busy_) == LOW; }

  bool reproducirCarpeta(uint8_t carpeta, uint8_t pista) {
    if (!listo_ || carpeta < 1 || carpeta > 99 || pista < 1) return false;
    enviar(0x0F, carpeta, pista);
    return true;
  }

  bool volumen(uint8_t valor) {
    if (!listo_) return false;
    if (valor > 30) valor = 30;
    enviar(0x06, 0, valor);
    return true;
  }

  void detener() { if (listo_) enviar(0x16, 0, 0); }

 private:
  void enviar(uint8_t comando, uint8_t alto, uint8_t bajo) {
    uint8_t trama[10] = {0x7E, 0xFF, 0x06, comando, 0x00, alto, bajo, 0, 0, 0xEF};
    uint16_t suma = 0;
    for (uint8_t i = 1; i <= 6; ++i) suma += trama[i];
    const uint16_t checksum = 0U - suma;
    trama[7] = uint8_t(checksum >> 8);
    trama[8] = uint8_t(checksum);
    serial_.write(trama, sizeof(trama));
  }

  HardwareSerial& serial_;
  int8_t busy_ = -1;
  bool listo_ = false;
};
