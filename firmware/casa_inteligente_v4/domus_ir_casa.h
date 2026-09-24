#pragma once
// Receptor IR del firmware de producto. Aprende las 21 teclas CAR MP3 y
// conserva el mapa en NVS. El pin se entrega en begin() para que MAPA_CASA
// siga siendo la unica fuente de GPIO.
#include <Arduino.h>
#include <Preferences.h>
#include <IRremote.hpp>

namespace IRCasa {
// Botones del mando CAR MP3 con sus códigos físicos capturados en la
// nota Obsidian 64 (protocolo P7). Cada botón tiene su carpeta de voz
// Jarvis propia: evento = índice + 1 (ver domus_jarvis_audio.h).
enum Tecla : uint8_t {
  CH_MENOS = 0,   // 0x45 modo manual
  CH = 1,         // 0x46 spare ON/OFF (LCD descartado, nota 80)
  CH_MAS = 2,     // 0x47 modo automático
  ANTERIOR = 3,   // 0x44 luz de sala
  PLAY = 4,       // 0x43 silencio
  SIGUIENTE = 5,  // 0x40 luz de cuarto
  VOL_MENOS = 6,  // 0x07 bajar volumen
  VOL_MAS = 7,    // 0x15 subir volumen
  EQ = 8,         // 0x09 diagnóstico
  N_0 = 9,        // 0x16 todo apagado
  N_100_MAS = 10,  // 0x19 alterna voz (un toque)
  N_200_MAS = 11,  // 0x0D rearme
  N_1 = 12,       // 0x0C luz de sala
  N_2 = 13,       // 0x18 luz de cuarto
  N_3 = 14,       // 0x5E luz de cultivo
  N_4 = 15,       // 0x08 todas las luces ON/OFF
  N_5 = 16,       // 0x1C riego
  N_6 = 17,       // 0x5A consulta temperatura
  N_7 = 18,       // 0x42 consulta humedad
  N_8 = 19,       // 0x52 consulta suelo + depósito
  N_9 = 20,       // 0x4A estado completo
  TOTAL = 21, NINGUNA = 255
};

constexpr uint16_t CODIGOS_INICIALES[TOTAL] = {
  0x45, 0x46, 0x47, 0x44, 0x43, 0x40, 0x07, 0x15, 0x09,
  0x16, 0x19, 0x0D, 0x0C, 0x18, 0x5E, 0x08, 0x1C,
  0x5A, 0x42, 0x52, 0x4A
};
constexpr const char* NOMBRES[TOTAL] = {
  "CH-", "CH", "CH+", "ANTERIOR", "PLAY", "SIGUIENTE",
  "VOL-", "VOL+", "EQ", "0", "100+", "200+",
  "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

struct Evento {
  bool hay = false;
  Tecla tecla = NINGUNA;
  bool repeticion = false;
  uint16_t codigo = 0;
  uint16_t direccion = 0;
  uint8_t protocolo = 0;
};

class Receptor {
 public:
  void begin(uint8_t pin) {
    preferencias_.begin("domus-ir", false);
    mascaraAprendida_ = preferencias_.getUInt("mask", 0) & MASCARA_TOTAL;
    for (uint8_t i = 0; i < TOTAL; ++i) {
      uint16_t valor = preferencias_.getUShort(clave(i), 0xFFFF);
      tabla_[i] = valor == 0xFFFF ? CODIGOS_INICIALES[i] : valor;
    }
    // NVS virgen (feria): confiar en CODIGOS_INICIALES (nota 64) sin
    // exigir IR_GRABAR. IR_BORRAR pone mapa_borrado y lo desactiva.
    if (mascaraAprendida_ == 0 && !preferencias_.getBool("mapa_borrado", false))
      mascaraAprendida_ = MASCARA_TOTAL;
    IrReceiver.begin(pin, DISABLE_LED_FEEDBACK);
  }

  Evento actualizar() {
    Evento evento;
    if (!IrReceiver.decode()) return evento;
    evento.codigo = IrReceiver.decodedIRData.command;
    evento.direccion = IrReceiver.decodedIRData.address;
    evento.protocolo = IrReceiver.decodedIRData.protocol;
    evento.repeticion =
      (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
    ultimoCodigo_ = evento.codigo;
    evento.tecla = buscar(evento.codigo);
    IrReceiver.resume();

    // Una pulsacion sostenida no repite acciones. Solo volumen admite repeat
    // limitado, aunque audio permanezca deshabilitado en el banco.
    if (evento.repeticion && evento.tecla != VOL_MENOS && evento.tecla != VOL_MAS)
      return Evento{};
    if (evento.repeticion) {
      if (millis() - ultimoVolumenMs_ < 150) return Evento{};
      ultimoVolumenMs_ = millis();
    }
    evento.hay = true;
    return evento;
  }

  bool grabar(uint8_t indice, uint16_t codigo) {
    ultimoError_ = "error_nvs";
    if (indice >= TOTAL || codigo == 0) { ultimoError_ = "codigo_invalido"; return false; }
    for (uint8_t i = 0; i < TOTAL; ++i) {
      if (i != indice && aprendida(i) && tabla_[i] == codigo) {
        ultimoError_ = "codigo_duplicado";
        return false;
      }
    }
    if (preferencias_.putUShort(clave(indice), codigo) != sizeof(uint16_t)) return false;
    const uint32_t nuevaMascara = mascaraAprendida_ | (1UL << indice);
    if (preferencias_.putUInt("mask", nuevaMascara) != sizeof(uint32_t)) return false;
    tabla_[indice] = codigo;
    mascaraAprendida_ = nuevaMascara;
    ultimoError_ = "ok";
    return true;
  }
  void borrar() {
    preferencias_.clear();
    preferencias_.putBool("mapa_borrado", true);
    mascaraAprendida_ = 0;
    for (uint8_t i = 0; i < TOTAL; ++i) {
      tabla_[i] = CODIGOS_INICIALES[i];
    }
  }
  uint16_t codigo(uint8_t indice) const { return indice < TOTAL ? tabla_[indice] : 0; }
  bool aprendida(uint8_t indice) const {
    return indice < TOTAL && (mascaraAprendida_ & (1UL << indice)) != 0;
  }
  uint8_t totalAprendidas() const {
    uint8_t total = 0;
    for (uint8_t i = 0; i < TOTAL; ++i) if (aprendida(i)) ++total;
    return total;
  }
  bool mapaCompleto() const { return totalAprendidas() == TOTAL; }
  const char* ultimoError() const { return ultimoError_; }
  uint16_t ultimoCodigo() const { return ultimoCodigo_; }
  static const char* nombre(uint8_t indice) { return indice < TOTAL ? NOMBRES[indice] : "?"; }

 private:
  Tecla buscar(uint16_t codigo) const {
    for (uint8_t i = 0; i < TOTAL; ++i)
      if (aprendida(i) && tabla_[i] == codigo) return static_cast<Tecla>(i);
    return NINGUNA;
  }
  const char* clave(uint8_t indice) {
    static char buffer[8];
    snprintf(buffer, sizeof(buffer), "k%02u", indice);
    return buffer;
  }
  Preferences preferencias_;
  static constexpr uint32_t MASCARA_TOTAL = (1UL << TOTAL) - 1UL;
  uint16_t tabla_[TOTAL] = {};
  uint32_t mascaraAprendida_ = 0;
  const char* ultimoError_ = "sin_error";
  uint16_t ultimoCodigo_ = 0;
  uint32_t ultimoVolumenMs_ = 0;
};
}  // namespace IRCasa
