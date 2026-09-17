#pragma once
#include <Arduino.h>
#include "domus_dfplayer.h"

enum class EventoJarvis : uint8_t {
  SISTEMA_LISTO = 1, ORDEN_ACEPTADA, ORDEN_RECHAZADA, LUZ_ENCENDIDA,
  LUZ_APAGADA, RIEGO_INICIADO, RIEGO_DETENIDO, TIERRA_SECA,
  TIERRA_HUMEDA, AGUA_BAJA, TEMPERATURA_ALTA, PRESENCIA, EMERGENCIA,
  ERROR_SENSOR, LUZ_CULTIVO_ENCENDIDA, LUZ_CULTIVO_APAGADA,
  VENTILADOR_ENCENDIDO, VENTILADOR_APAGADO, AIRE_SECO, AIRE_HUMEDO,
  MODOS_ON, MODOS_OFF, PERSONALIDAD_CARLOS, PERSONALIDAD_KARLA,
  CATEGORIA_RIEGO, CATEGORIA_LUZ, CATEGORIA_VENTILADOR,
  CATEGORIA_AMBIENTE, CATEGORIA_SEGURIDAD, CATEGORIA_SALUD
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
    if (!habilitado_ || silenciado_ || eventoBase < 1) return false;
    // Mapeo de eventos a carpetas: 1-14 en folders 01-14 (voz 1), 15-28 en folders 15-28 (voz 1),
    // 29-42 en folders 29-42 (voz 2), 43-56 en folders 57-70 (voz 2), 57-70 en folders 71-84
    const uint8_t carpetaBase = eventoBase <= 14 ? eventoBase :
                                eventoBase <= 28 ? eventoBase - 14 :
                                eventoBase <= 42 ? eventoBase + 14 :
                                eventoBase <= 56 ? eventoBase + 27 :
                                                   eventoBase + 56;
    const uint8_t carpeta = voz_ == 2 ? carpetaBase + 50U : carpetaBase;
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
