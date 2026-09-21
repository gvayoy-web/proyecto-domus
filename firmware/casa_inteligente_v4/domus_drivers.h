#pragma once
// PROJECT DOMUS — backends de driver de motores.
//
// Seleccion por bandera de compilacion:
//   -DDOMUS_DRIVER=0 -> NINGUNO (seguro, predeterminado)
//   -DDOMUS_DRIVER=1 -> DRV8833 (preparado, activo en perfil 4)
//   -DDOMUS_DRIVER=2 -> MX1508
//   -DDOMUS_DRIVER_VALIDADO=1 -> F1 acreditada físicamente
// El despachador de casa_inteligente_v4.ino no cambia: `driver_no_listo`
// se revisa antes que `salida_no_instalada`.
//
// El dispatch de motores (driverMotoresAplicar) vive en el .ino principal
// para permitir que el backend solo declare constantes y estado.
//
// Seleccion por bandera de compilacion:
//   -DDOMUS_DRIVER=0 -> NINGUNO (seguro, predeterminado)
//   -DDOMUS_DRIVER=1 -> DRV8833 (preparado, activo en perfil 4)
//   -DDOMUS_DRIVER=2 -> MX1508
//   -DDOMUS_DRIVER_VALIDADO=1 -> F1 acreditada físicamente
// Otro valor falla por static_assert.
#include <stdint.h>

#ifndef DOMUS_PERFIL_CASA
#define DOMUS_PERFIL_CASA 4
#endif
#ifndef DOMUS_DRIVER
#define DOMUS_DRIVER 0
#endif
#ifndef DOMUS_DRIVER_VALIDADO
#define DOMUS_DRIVER_VALIDADO 0
#endif

// Backend de driver separado, seleccionable por bandera.
enum class BackendMotor : uint8_t { NINGUNO = 0, DRV8833 = 1, MX1508 = 2 };

static_assert(DOMUS_DRIVER >= 0 && DOMUS_DRIVER <= 2,
               "DOMUS_DRIVER debe ser 0, 1 o 2");
static_assert(DOMUS_DRIVER_VALIDADO == 0 || DOMUS_DRIVER_VALIDADO == 1,
               "DOMUS_DRIVER_VALIDADO debe ser 0 o 1");

constexpr BackendMotor BACKEND_MOTOR_SELECCIONADO =
    DOMUS_DRIVER == 1 ? BackendMotor::DRV8833 :
    DOMUS_DRIVER == 2 ? BackendMotor::MX1508 :
                        BackendMotor::NINGUNO;

constexpr const char* nombreBackendMotor(BackendMotor b) {
  return b == BackendMotor::DRV8833 ? "DRV8833" :
         b == BackendMotor::MX1508 ? "MX1508" : "NINGUNO";
}

// --- Perfil con DRV8833 ---
constexpr bool PERFIL_CON_DRV8833 =
    DOMUS_PERFIL_CASA == 4;

// --- Driver de motores (puerta F1) ---
// DRIVER_MOTORES_LISTO requiere backend seleccionado + F1 acreditada + perfil 4.
constexpr bool DRIVER_MOTORES_LISTO =
    BACKEND_MOTOR_SELECCIONADO != BackendMotor::NINGUNO &&
    DOMUS_DRIVER_VALIDADO == 1 &&
    PERFIL_CON_DRV8833;

// GPIO asignados al DRV8833 (perfil CASA_FINAL_DRV8833_DFPLAYER).
// AIN1=GPIO4 (bomba), AIN2=GPIO7 (bomba)
// BIN1/BIN2: ELIMINADOS - ventilador muerto, GPIO5/6 libres para otros usos
// nSLEEP → VCC (no usa GPIO)
constexpr uint8_t DRV8833_PIN_AIN1 = 4;
constexpr uint8_t DRV8833_PIN_AIN2 = 7;
// constexpr uint8_t DRV8833_PIN_BIN1 = 5;  // ELIMINADO: ventilador muerto
// constexpr uint8_t DRV8833_PIN_BIN2 = 6;  // ELIMINADO: ventilador muerto

// Orden que el driver confirmado ejecutará.
struct OrdenMotorDriver {
  uint8_t canal;  // 0 = bomba, 1 = ventilador
  bool activar;
};

bool driverMotoresAplicar(uint8_t canal, bool activar) {
  (void)canal;
  (void)activar;
  return false;
}

inline bool driverMotoresListo() { return DRIVER_MOTORES_LISTO; }

// Dispatch de motores: vivo en el .ino principal.
// El despachador llama a driverMotoresAplicar() solo si DRIVER_MOTORES_LISTO.

// --- Descriptores por backend ---
struct DescriptorBackendMotor {
  const char* nombre;
  const char* notasCableadoFuturo;
};
constexpr DescriptorBackendMotor DESCRIPTOR_NINGUNO = {
  "NINGUNO",
  "Sin driver: ningun modulo adivinado; motores bloqueados hasta F1."
};
constexpr DescriptorBackendMotor DESCRIPTOR_DRV8833 = {
  "DRV8833",
  "Puente dual con entradas AIN1/AIN2/BIN1/BIN2 y pin nSLEEP a VCC. "
  "GPIO: AIN1=4, AIN2=7, BIN1=5, BIN2=6."
};
constexpr DescriptorBackendMotor DESCRIPTOR_MX1508 = {
  "MX1508",
  "Puente dual con entradas IN1-IN4 y salidas OUT1-OUT4, sin pin SLEEP; cableado futuro pendiente de F1, sin GPIO asignados."
};
constexpr const DescriptorBackendMotor& descriptorBackendMotor(BackendMotor b) {
  return b == BackendMotor::DRV8833 ? DESCRIPTOR_DRV8833 :
         b == BackendMotor::MX1508 ? DESCRIPTOR_MX1508 : DESCRIPTOR_NINGUNO;
}

// El IR ya no pertenece a este archivo de drivers de motor. Su implementación
// y tabla aprendible viven en domus_ir_casa.h; el perfil activo decide si usa
// GPIO12. Esto evita mantener dos banderas contradictorias.

// --- Audio Jarvis ---
// DFPlayer Mini por UART. El perfil CASA_FINAL_DRV8833_DFPLAYER habilita
// el audio. Los pines UART se definen en el .ino principal.
constexpr bool AUDIO_CANDIDATO_HABILITADO =
    PERFIL_CON_DRV8833 && DOMUS_DRIVER_VALIDADO == 1;
