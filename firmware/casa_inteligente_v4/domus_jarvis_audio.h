#pragma once
#include <Arduino.h>
#include "domus_dfplayer.h"

// Catálogo Jarvis 1:1 con los 21 botones del mando CAR MP3.
// Cada evento vive en su carpeta: voz 1 (Carlos) 01-21, voz 2 (Karla) 51-71
// (desplazamiento +50). Códigos físicos capturados en la nota Obsidian 64
// (protocolo P7); acciones por botón en la nota 46; DFPlayer exige carpetas
// 01-99 con pistas 001-004 (nota 65).
//
// Convención de variantes (001-004) por carpeta:
// - Conmutadores (sala, cuarto, cultivo, ventilador, riego): 1 = ON manual,
//   2 = OFF manual, 3 = ON automático, 4 = OFF automático.
// - Botón 0: 1-2 = apagado general; 3-4 = PARO de emergencia.
// - EQ: 1-2 = diagnóstico manual; 3-4 = fallo de sensor (automático).
// - Tecla 8 (suelo): 1-2 = consulta en pantalla; 3 = tierra seca (reservada
//   para alerta automática); 4 = depósito bajo, riego bloqueado.
// - Tecla 9 (estado): las 4 = estado general; la 3 = "Sistemas en línea"
//   (arranque y confirmación de cambio de voz).
// - Tecla 200+: 1-2 = rearme logrado; 3-4 = sigue bloqueado.
// - Tecla 100+: 1-2 = "repitiendo"; 3-4 = "nada que repetir".
// - Resto (CH-, CH, CH+, Play, VOL-, VOL+, 6, 7): 4 variantes del mismo
//   significado; Play usa paridad ON {1,3} / OFF {2,4}.
enum class EventoJarvis : uint8_t {
  CH_MENOS = 1,    // CH- 0x45: modo manual
  CH = 2,          // CH  0x46: página siguiente del LCD
  CH_MAS = 3,      // CH+ 0x47: modo automático
  ANTERIOR = 4,    // Anterior 0x44: luz de sala (comparte con tecla 1)
  PLAY = 5,        // Play 0x43: silencio on/off
  SIGUIENTE = 6,   // Siguiente 0x40: luz de cuarto (comparte con tecla 2)
  VOL_MENOS = 7,   // VOL- 0x07: bajar volumen
  VOL_MAS = 8,     // VOL+ 0x15: subir volumen
  EQ = 9,          // EQ 0x09: diagnóstico
  TECLA_0 = 10,    // 0 0x16: todo apagado (+ PARO físico)
  TECLA_100 = 11,  // 100+ 0x19: repetir última pista
  TECLA_200 = 12,  // 200+ 0x0D: rearme seguro
  TECLA_1 = 13,    // 1 0x0C: luz de sala
  TECLA_2 = 14,    // 2 0x18: luz de cuarto
  TECLA_3 = 15,    // 3 0x5E: luz de cultivo
  TECLA_4 = 16,    // 4 0x08: ventilador
  TECLA_5 = 17,    // 5 0x1C: riego
  TECLA_6 = 18,    // 6 0x5A: consulta temperatura (doble = cambio de voz)
  TECLA_7 = 19,    // 7 0x42: consulta humedad
  TECLA_8 = 20,    // 8 0x52: consulta suelo + depósito
  TECLA_9 = 21     // 9 0x4A: estado completo (+ arranque)
};

// Política no bloqueante de Jarvis: variantes sin repetición inmediata.
// Las alertas automáticas usan enfriamiento de 30 s; el PARO interrumpe
// cualquier frase y siempre tiene prioridad.
class JarvisAudio {
 public:
  static constexpr uint8_t NUM_EVENTOS = 21;
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

  // Carpeta DFPlayer para un evento y voz dados. Voz 1: 01-21, voz 2: 51-71.
  static uint8_t carpetaPara(EventoJarvis evento, uint8_t voz) {
    const uint8_t base = static_cast<uint8_t>(evento);
    if (base < 1 || base > NUM_EVENTOS) return 0;
    return voz == 2 ? uint8_t(base + DESPLAZAMIENTO_VOZ_2) : base;
  }

  // variante: 0 = aleatoria 1-4; 1-4 = pista fija (p. ej. 1 = ON, 2 = OFF).
  bool reproducir(EventoJarvis evento, uint32_t ahoraMs,
                  bool alertaAutomatica = false, uint8_t variante = 0) {
    const uint8_t eventoBase = static_cast<uint8_t>(evento);
    if (!habilitado_ || silenciado_ || eventoBase < 1 || eventoBase > NUM_EVENTOS)
      return false;
    if (variante > 4) return false;
    const uint8_t carpeta = carpetaPara(evento, voz_);
    const bool emergencia = evento == EventoJarvis::TECLA_0 && variante >= 3;
    if (!emergencia && transporte_.ocupado()) return false;
    if (alertaAutomatica && !emergencia &&
        ahoraMs - ultimoEventoMs_[eventoBase] < ENFRIAMIENTO_ALERTA_MS) return false;
    if (emergencia) transporte_.detener();

    uint8_t pista;
    if (variante != 0) {
      pista = variante;
    } else {
      pista = 1 + uint8_t(esp_random() % 4U);
      if (pista == ultimaPista_[eventoBase]) pista = uint8_t((pista % 4U) + 1U);
    }
    if (!transporte_.reproducirCarpeta(carpeta, pista)) return false;
    ultimaPista_[eventoBase] = pista;
    ultimoEventoMs_[eventoBase] = ahoraMs;
    ultimoEvento_ = evento;
    ultimaCarpetaReproducida_ = carpeta;
    ultimaPistaReproducida_ = pista;
    return true;
  }

  // Aleatoria entre dos variantes (p. ej. grupo(1,2) = par ON/OFF manual),
  // evitando repetir la misma pista dos veces seguidas.
  bool reproducirGrupo(EventoJarvis evento, uint32_t ahoraMs,
                       bool alertaAutomatica, uint8_t vA, uint8_t vB) {
    if (vA < 1 || vA > 4 || vB < 1 || vB > 4 || vA == vB) return false;
    const uint8_t eventoBase = static_cast<uint8_t>(evento);
    const uint8_t elegida =
        (ultimaPista_[eventoBase] == vA) ? vB
        : (ultimaPista_[eventoBase] == vB) ? vA
        : (esp_random() % 2U == 0 ? vA : vB);
    return reproducir(evento, ahoraMs, alertaAutomatica, elegida);
  }

  // Conmutador con alternas: ON rota {1,3}, OFF rota {2,4} (carpeta Play).
  bool reproducirEstado(EventoJarvis evento, uint32_t ahoraMs,
                        bool alertaAutomatica, bool encendido) {
    const uint8_t eventoBase = static_cast<uint8_t>(evento);
    if (eventoBase < 1 || eventoBase > NUM_EVENTOS) return false;
    const uint8_t base = encendido ? 1 : 2;
    const uint8_t alt = uint8_t(base + 2U);
    const uint8_t elegida = (ultimaPista_[eventoBase] == base) ? alt : base;
    return reproducir(evento, ahoraMs, alertaAutomatica, elegida);
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
  EventoJarvis ultimoEvento_ = EventoJarvis::CH_MENOS;
  uint8_t ultimaCarpetaReproducida_ = 0;
  uint8_t ultimaPistaReproducida_ = 0;
  uint8_t volumen_ = 18;
  uint8_t voz_ = 1;
  bool habilitado_ = false;
  bool silenciado_ = false;
};
