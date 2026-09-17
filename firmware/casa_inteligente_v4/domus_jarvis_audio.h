#pragma once
#include <Arduino.h>
#include "domus_dfplayer.h"

// Catálogo Jarvis 1:1 con las 21 teclas CAR MP3 (nota Obsidian 46) más
// sensores. Cada evento tiene carpeta propia 01-28 en voz 1 (Carlos) y
// 51-78 en voz 2 (Karla, desplazamiento +50). DFPlayer exige carpetas
// 01-99 con pistas 001-004 (nota 65).
enum class EventoJarvis : uint8_t {
  SISTEMA_LISTO = 1,      // 01 arranque / cambio de voz confirmado
  ORDEN_ACEPTADA = 2,     // 02 confirmación genérica
  ORDEN_RECHAZADA = 3,    // 03 rechazo genérico / riego rechazado
  LUZ_ENCENDIDA = 4,      // 04 sala o cuarto encendidos (teclas 1/2, Anterior/Siguiente)
  LUZ_APAGADA = 5,        // 05 sala o cuarto apagados
  RIEGO_INICIADO = 6,     // 06 tecla 5 aceptada
  RIEGO_DETENIDO = 7,     // 07 riego detenido / timeout
  TIERRA_SECA = 8,        // 08 alerta automática suelo seco
  TIERRA_HUMEDA = 9,      // 09 suelo recuperado
  AGUA_BAJA = 10,         // 10 riego bloqueado por nivel
  TEMPERATURA_ALTA = 11,  // 11 alerta térmica / auto ventilador
  PRESENCIA = 12,         // 12 alerta PIR
  EMERGENCIA = 13,        // 13 PARO, interrumpe todo, sin enfriamiento
  ERROR_SENSOR = 14,      // 14 sensor inválido
  MODO_MANUAL = 15,       // 15 tecla CH-
  MODO_AUTO = 16,         // 16 tecla CH+
  DIAGNOSTICO = 17,       // 17 tecla EQ
  TODO_APAGADO = 18,      // 18 tecla 0
  SONIDO_ACTIVADO = 19,   // 19 PLAY / 100+ (silencio on/off, repetir)
  SISTEMA_REARMADO = 20,  // 20 tecla 200+
  LUZ_CULTIVO_ENCENDIDA = 21,  // 21 tecla 3 on
  LUZ_CULTIVO_APAGADA = 22,    // 22 tecla 3 off
  VENTILADOR_ENCENDIDO = 23,   // 23 tecla 4 on
  VENTILADOR_APAGADO = 24,     // 24 tecla 4 off
  CONSULTA_TEMP = 25,     // 25 tecla 6
  CONSULTA_HUMEDAD = 26,  // 26 tecla 7
  CONSULTA_SUELO = 27,    // 27 tecla 8 (suelo + depósito)
  ESTADO_COMPLETO = 28    // 28 tecla 9 / CH+
};

// Política no bloqueante de Jarvis: cuatro variantes por carpeta, sin repetir
// inmediatamente. Las alertas automáticas usan enfriamiento de 30 segundos;
// EMERGENCIA interrumpe cualquier frase y siempre tiene prioridad.
class JarvisAudio {
 public:
  static constexpr uint8_t NUM_EVENTOS = 28;
  static constexpr uint8_t DESPLAZAMIENTO_VOZ_2 = 50;

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

  // Carpeta DFPlayer para un evento y voz dados. Voz 1: 01-28, voz 2: 51-78.
  static uint8_t carpetaPara(EventoJarvis evento, uint8_t voz) {
    const uint8_t base = static_cast<uint8_t>(evento);
    if (base < 1 || base > NUM_EVENTOS) return 0;
    return voz == 2 ? uint8_t(base + DESPLAZAMIENTO_VOZ_2) : base;
  }

  bool reproducir(EventoJarvis evento, uint32_t ahoraMs,
                  bool alertaAutomatica = false) {
    const uint8_t eventoBase = static_cast<uint8_t>(evento);
    if (!habilitado_ || silenciado_ || eventoBase < 1 || eventoBase > NUM_EVENTOS)
      return false;
    const uint8_t carpeta = carpetaPara(evento, voz_);
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
  uint32_t ultimoEventoMs_[NUM_EVENTOS + 1] = {};
  uint8_t ultimaPista_[NUM_EVENTOS + 1] = {};
  EventoJarvis ultimoEvento_ = EventoJarvis::SISTEMA_LISTO;
  uint8_t ultimaCarpetaReproducida_ = 0;
  uint8_t ultimaPistaReproducida_ = 0;
  uint8_t volumen_ = 18;
  uint8_t voz_ = 1;
  bool habilitado_ = false;
  bool silenciado_ = false;
};
