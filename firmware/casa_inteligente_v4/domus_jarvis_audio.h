#pragma once
#include <Arduino.h>
#include "domus_dfplayer.h"

enum class EventoJarvis : uint8_t {
  SISTEMA_LISTO = 1, ORDEN_ACEPTADA, ORDEN_RECHAZADA, LUZ_ENCENDIDA,
  LUZ_APAGADA, RIEGO_INICIADO, RIEGO_DETENIDO, TIERRA_SECA,
  TIERRA_HUMEDA, AGUA_BAJA, TEMPERATURA_ALTA, PRESENCIA, EMERGENCIA,
  ERROR_SENSOR
};

// Política no bloqueante de Jarvis: cuatro variantes por carpeta, sin repetir
// inmediatamente. Las alertas automáticas usan enfriamiento de 30 segundos;
// EMERGENCIA interrumpe cualquier frase y siempre tiene prioridad.
class JarvisAudio {
 public:
  explicit JarvisAudio(DFPlayerTransport& transporte) : transporte_(transporte) {}

  void begin(uint8_t volumenInicial = 18) {
    volumen_ = volumenInicial > 30 ? 30 : volumenInicial;
    habilitado_ = transporte_.listo() && transporte_.volumen(volumen_);
  }

  bool habilitado() const { return habilitado_; }
  bool silenciado() const { return silenciado_; }
  uint8_t volumenActual() const { return volumen_; }
  uint8_t vozActual() const { return voz_; }
  void silenciar(bool valor) { silenciado_ = valor; if (valor) transporte_.detener(); }

  bool cambiarVoz() {
    voz_ = voz_ == 1 ? 2 : 1;
    return habilitado_;
  }

  bool ajustarVolumen(int8_t delta) {
    int nuevo = int(volumen_) + delta;
    if (nuevo < 0) nuevo = 0;
    if (nuevo > 30) nuevo = 30;
    volumen_ = uint8_t(nuevo);
    return habilitado_ && transporte_.volumen(volumen_);
  }

  bool reproducir(EventoJarvis evento, uint32_t ahoraMs,
                  bool alertaAutomatica = false) {
    const uint8_t eventoBase = static_cast<uint8_t>(evento);
    if (!habilitado_ || silenciado_ || eventoBase < 1 || eventoBase > 14) return false;
    const uint8_t carpeta = uint8_t(eventoBase + (voz_ == 2 ? 50U : 0U));
    const bool emergencia = evento == EventoJarvis::EMERGENCIA;
    if (!emergencia && transporte_.ocupado()) return false;
    if (alertaAutomatica && !emergencia &&
        ahoraMs - ultimoEventoMs_[eventoBase] < ENFRIAMIENTO_ALERTA_MS) return false;
    if (emergencia) transporte_.detener();

    uint8_t pista = 1 + uint8_t(esp_random() % 4U);
    if (pista == ultimaPista_[eventoBase]) pista = uint8_t((pista % 4U) + 1U);
    if (!transporte_.reproducirCarpeta(carpeta, pista)) return false;
    ultimaPista_[eventoBase] = pista;
    ultimoEventoMs_[eventoBase] = ahoraMs;
    ultimoEvento_ = evento;
    ultimaCarpetaReproducida_ = carpeta;
    ultimaPistaReproducida_ = pista;
    return true;
  }

  bool repetirUltima(uint32_t ahoraMs) {
    (void)ahoraMs;
    if (!habilitado_ || silenciado_ || transporte_.ocupado() ||
        ultimaCarpetaReproducida_ == 0 || ultimaPistaReproducida_ == 0) return false;
    return transporte_.reproducirCarpeta(ultimaCarpetaReproducida_,
                                         ultimaPistaReproducida_);
  }

 private:
  static constexpr uint32_t ENFRIAMIENTO_ALERTA_MS = 30000UL;
  DFPlayerTransport& transporte_;
  uint32_t ultimoEventoMs_[15] = {};
  uint8_t ultimaPista_[15] = {};
  EventoJarvis ultimoEvento_ = EventoJarvis::SISTEMA_LISTO;
  uint8_t ultimaCarpetaReproducida_ = 0;
  uint8_t ultimaPistaReproducida_ = 0;
  uint8_t volumen_ = 18;
  uint8_t voz_ = 1;
  bool habilitado_ = false;
  bool silenciado_ = false;
};
