/*
  ============================================================================
  PROJECT DOMUS - FIRMWARE v6 OFFLINE
  ============================================================================
  Placa: ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM OPI)
  Autor: Isaac | Proyecto de feria de ciencias

  NOTA DE MIGRACIÓN:
  Este firmware es ahora la única fuente de control. DOMUS no usa micrófono,
  reconocimiento de voz, IA ni TinyML. "Jarvis" es la personalidad de las
  respuestas fijas disparadas por mando IR; el audio se integrará después.

  ============================================================================
  POR QUÉ CAMBIÓ TODO ESTE ARCHIVO RESPECTO A v3
  ============================================================================
  v3 usaba BluetoothSerial (Bluetooth Classic / SPP). El ESP32-S3 NO tiene
  radio de Bluetooth Classic, solo Bluetooth LE. Espressif lo documenta
  explícitamente, y el propio core Arduino-ESP32 no compila BluetoothSerial
  para variantes S3. Es decir: v3 no podía funcionar en este hardware, sin
  importar cuánto se depurara el resto del código.

  v6 elimina las dependencias de aplicación móvil y BLE. El mismo vocabulario
  de comandos (RIEGO_ON, ESTADO, etc.) queda disponible localmente por USB
  Serial para diagnóstico y pruebas, además de los controles físicos.

  CAMBIOS PRINCIPALES DE v6:
    - ACK/NACK REAL POR COMANDO: cada comando de control ahora responde
      "ACK;<comando>;<estado_logico_gpio>" o "NACK;<comando>;<motivo>"
      inmediatamente por Serial, sin transporte de red.
    - HUMEDAD CALIBRADA EN PORCENTAJE: además del valor crudo ADC, el
      firmware ahora expone HUM_PCT (0-100%) usando dos constantes de
      calibración (tierra seca / tierra húmeda) en vez de mostrar un
      número crudo como "2437" sin significado para el usuario.
    - COLA DE COMANDOS NO APLICA AQUÍ: el firmware siempre fue un simple
      receptor/ejecutor, la cola con TTL vive del lado de la app (ver
      La cola de comandos no vive en el firmware: este archivo recibe y ejecuta,
      pero si recibe un comando corrupto lo rechaza con NACK en vez
      de solo loguearlo, para que la app pueda reaccionar en vivo.
    - SENSOR DE TEMPERATURA/HUMEDAD AMBIENTAL (DHT11): nuevo sensor físico,
      separado del sensor de humedad de TIERRA que ya existía. El DHT11 mide
      el aire (°C y %HR ambiental), no la tierra de la maceta - son dos
      magnitudes físicas distintas y así se documentan por separado en
      ESTADO (TEMP_C / HUM_AIRE_PCT vs HUM_PCT de tierra). Se usa además
      para automatizar el ventilador por temperatura, no solo por comando
      manual.
    - SENSOR DE LUZ AMBIENTAL (LDR / fotoresistor): nuevo sensor físico,
      expuesto como LUZ_PCT (0-100%, calibrado igual que la humedad de
      tierra) y usado para decidir automáticamente si tiene sentido
      encender las luces (evita, por ejemplo, prender "Luz Sala" en pleno
      día si el usuario la dejó en automático).

  Todo lo de v3 se mantiene igual: watchdog, verificación de estado lógico
  de relés (renombrada para no prometer más de lo que hace), sistema de
  errores circular, recuperación I2C, detección automática del LCD1602,
  riego automático, módulo MP3.

  ============================================================================
  HERRAMIENTAS ACTUALES (Arduino IDE, firmware base):
    Board: "ESP32S3 Dev Module"
    PSRAM: "OPI PSRAM"
    Flash Size: "16MB"
    Partition Scheme: app3M_fat9M_16MB para la compilación N16R8 validada.

  LIBRERÍAS A INSTALAR (ver versiones exactas probadas en el README):
    1. "LiquidCrystal I2C"   - Frank de Brabander (para BLOQUE LCD)
    2. "DHT sensor library"  - Adafruit (para el DHT11 de temperatura/humedad)
    3. "Adafruit Unified Sensor" - Adafruit (dependencia de DHT sensor library)

  AUDIO:
    - aplazado hasta definir una fuente de señal compatible con MAX98306;
    - no existe ni se requiere una ruta de reconocimiento de voz.
  ============================================================================
*/

// MIGRACIÓN FASE E PASO 1 (nota 46, nota 52): la abstracción de relés se
// sustituye por SalidaDomus (TOTAL_SALIDAS, MAPA_CASA.salidas, estadoSalidas,
// propietarioSalidas, solicitar/desactivarSalida, verificarNivelLogicoSalida).
// Renombre mecánico, cero cambios de comportamiento: reglas de seguridad,
// histéresis, propiedad manual y protocolo quedan intactos. Pasos pendientes:
// drivers por perfil (FINAL), IR como entrada, audio por cola y validación
// física. El esqueleto alfa sigue siendo el firmware de banco vigente.
// Estado formal: CANDIDATO (ver nota 53); FINAL solo tras puertas F1-F7.

// ÍNDICE DE SECCIONES (orden del fichero):
// 1 pines (MAPA_CASA) · 2 calibración · 3/3B globales y errores ·
// 5 log · 5B/5C microSD e historial · 6 LCD/I2C · 7 MP3/Jarvis ·
// 8/8B relés y pantalla · 9 sensores · 10/10B riego/ventilador auto ·
// 11 Serial+IR+CAL (dispatcher por tabla) · 12B salud · 13 setup ·
// 13B demo y temporizadores · 13C telemetría SENSORES; · 14 loop.
// Módulos propios: domus_types / calibration / drivers / ir_casa /
// dfplayer / jarvis_audio / pantalla (solo Arduino + LCD).

#ifndef MICROSD_HABILITADA
#define MICROSD_HABILITADA false
#endif

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <limits.h>
#include <Preferences.h>
#include <new>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "domus_types.h"
#include "domus_calibration.h"
#include "domus_drivers.h"
#include "domus_ir_casa.h"
#include "domus_dfplayer.h"
#include "domus_jarvis_audio.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"   // watchdog de hardware
#include "esp_system.h"     // esp_get_free_heap_size(), esp_restart()

// ---- Pantalla oficial del proyecto ----
#include <LiquidCrystal_I2C.h>
#include "domus_pantalla.h"

// ---- Sensor de temperatura/humedad ambiental ----

;

// ============================================================================
// SECCIÓN 1: MAPA DE PINES - REVISAR/CALIBRAR A MANO
// ============================================================================
// Los sensores analógicos permanecen en ADC1 para reducir conflictos.

// --- Bus I2C compartido (pantalla, sea cual sea) ---
// Direcciones esperadas (se detectan automáticamente, no hace falta tocarlas):
#define DIR_LCD_1       0x27
#define DIR_LCD_2       0x3F

// --- Sensores y controles: VALORES SOLO EN MAPA_CASA (abajo) ---
// Costado accesible autorizado (nota 46/47/55). No existen #define de pines
// del candidato fuera de MAPA_CASA: cualquier GPIO nuevo entra por el struct.
// RECORDATORIO FÍSICO (LDR): el fotoresistor va en divisor de voltaje con
// una resistencia fija (típicamente 10k) entre 3.3V y GND; MAPA_CASA.ldr lee el
// punto medio del divisor, nunca el LDR solo contra 3.3V.

// --- Sensor de temperatura/humedad AMBIENTAL (DHT11) ---
// Distinto del sensor de humedad de TIERRA (suelo): el DHT11 mide el
// aire alrededor de la maceta/casa, no la tierra dentro de ella.

// --- Cinco salidas con su etapa física según perfil (nota 53/54) ---
// Bomba y ventilador exigen driver confirmado (F1) y permanecen bloqueados en
// los tres perfiles actuales. Las tres luces son LED individuales con
// resistencia propia y son activas en HIGH en el perfil económico.
// Si se instala una etapa distinta, calibrar esta tabla y el perfil.
#define TOTAL_SALIDAS 5

// Perfil 4 es CASA_FINAL_DRV8833_DFPLAYER: DRV8833 para bomba y
// ventilador, luces directas por GPIO (sin 74HC595), DFPlayer por SoftwareSerial.
// Perfil 3 es el banco actual: usa el mismo firmware de producto con una
// bomba por S8050, luces LED e infrarrojo; el driver doble y el audio quedan
// fuera hasta que llegue el hardware. Cambiar a 0 restaura banco sin salidas.
#ifndef DOMUS_PERFIL_CASA
#define DOMUS_PERFIL_CASA 4
#endif

// Mantener 0 para el cableado original. Seleccionar 1 únicamente
// después de montar la alternativa descrita en la nota 21 de Obsidian.
#ifndef DOMUS_SALIDAS_ECONOMICAS
#define DOMUS_SALIDAS_ECONOMICAS 0
#endif
const bool SALIDA_ACTIVA_EN_BAJO[TOTAL_SALIDAS] = {
  DOMUS_PERFIL_CASA == 3 ? false : true,
  DOMUS_PERFIL_CASA == 3 ? false : !DOMUS_SALIDAS_ECONOMICAS,
  DOMUS_PERFIL_CASA == 3 ? false : !DOMUS_SALIDAS_ECONOMICAS,
  !DOMUS_SALIDAS_ECONOMICAS,
  DOMUS_PERFIL_CASA == 3 ? false : !DOMUS_SALIDAS_ECONOMICAS
};
const char* NOMBRES_SALIDAS[TOTAL_SALIDAS] = {
  "Bomba", "Casa", "Porche", "Cultivo", "Spare"
};

// --- Perfil del candidato y mapa central (nota 53/54, jefatura) ---
// 0 = BANCO_SIN_ACTUADORES (todo bloqueado). 1 = LED_SIN_MOTORES.
// 2 = MOTOR_PENDIENTE_DRIVER (motores bloqueados hasta F1; declara la
// intención del cableado futuro, no habilita nada hoy). El nombre de producto
// final no se usa en ningún binario (ver puertas F1-F7). Selección por
// bandera -DDOMUS_PERFIL_CASA=N.
enum class PerfilCasa : uint8_t {
   BANCO_SIN_ACTUADORES, LED_SIN_MOTORES, MOTOR_PENDIENTE_DRIVER,
   BANCO_COMPLETO_S8050_IR, CASA_FINAL_DRV8833_DFPLAYER
 };
 static_assert(DOMUS_PERFIL_CASA >= 0 && DOMUS_PERFIL_CASA <= 4,
               "DOMUS_PERFIL_CASA debe ser 0, 1, 2, 3 o 4");
 constexpr PerfilCasa PERFIL_CASA =
     DOMUS_PERFIL_CASA == 1 ? PerfilCasa::LED_SIN_MOTORES :
     DOMUS_PERFIL_CASA == 2 ? PerfilCasa::MOTOR_PENDIENTE_DRIVER :
     DOMUS_PERFIL_CASA == 3 ? PerfilCasa::BANCO_COMPLETO_S8050_IR :
     DOMUS_PERFIL_CASA == 4 ? PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER :
                              PerfilCasa::BANCO_SIN_ACTUADORES;
 constexpr const char* nombrePerfilCasa(PerfilCasa p) {
   return p == PerfilCasa::BANCO_COMPLETO_S8050_IR ? "BANCO_COMPLETO_S8050_IR" :
          p == PerfilCasa::LED_SIN_MOTORES ? "CANDIDATO_LED_SIN_MOTORES" :
          p == PerfilCasa::MOTOR_PENDIENTE_DRIVER ? "CANDIDATO_MOTOR_PENDIENTE_DRIVER" :
          p == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER ? "CASA_FINAL_DRV8833_DFPLAYER" :
          "CANDIDATO_BANCO_SIN_ACTUADORES";
 }
 // Nombre corto para el LCD 16x2 (vista 6): el largo se trunca a "PERFIL:BANCO_COM".
 constexpr const char* nombrePerfilCortoCasa(PerfilCasa p) {
   return p == PerfilCasa::BANCO_COMPLETO_S8050_IR ? "BANCO_S8050_IR" :
          p == PerfilCasa::LED_SIN_MOTORES ? "LED" :
          p == PerfilCasa::MOTOR_PENDIENTE_DRIVER ? "MOTOR_PEND" :
          p == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER ? "DRV8833_DF" :
          "SIN_ACT";
 }
// Mapa GPIO central: ÚNICA fuente de pines del candidato (nota 55).
// Costado accesible autorizado (nota 46/47): suelo 15, nivel 16, SDA 17,
// demo/modo 18. Todo el firmware lee MAPA_CASA; no existen #define de pines.
struct MapaPinesCasa {
  int suelo, nivel, ldr;
  int bomba, casa, porche, cultivo, spare;
  int paro, micOff, demo, scl, dht, sda, ir;  // micOff=GPIO9 (SILENCIO), demo=GPIO18 (tambien DFPlayer TX); LCD quemado: scl/sda libres para DFPlayer RX
  int salidas[TOTAL_SALIDAS];
};
constexpr MapaPinesCasa MAPA_CASA = {
  15, 16, 3,
  4, 5, 8, 7, 6,
  10, 9, 18, 13, 14, 17, 12,
  {4, 5, 8, 7, 6}
};
static_assert(MAPA_CASA.bomba == 4 && MAPA_CASA.casa == 5 && MAPA_CASA.porche == 8 &&
              MAPA_CASA.cultivo == 7 && MAPA_CASA.spare == 6, "Mapa de salidas del candidato");
static_assert(MAPA_CASA.suelo == 15 && MAPA_CASA.nivel == 16 && MAPA_CASA.sda == 17 &&
              MAPA_CASA.demo == 18 && MAPA_CASA.scl == 13 && MAPA_CASA.ir == 12,
              "Costado accesible autorizado");
static_assert(MAPA_CASA.bomba == MAPA_CASA.salidas[0] && MAPA_CASA.casa == MAPA_CASA.salidas[1] &&
              MAPA_CASA.porche == MAPA_CASA.salidas[2] && MAPA_CASA.cultivo == MAPA_CASA.salidas[3] &&
              MAPA_CASA.spare == MAPA_CASA.salidas[4], "Campos y arreglo de salidas unidos");
// Habilitación física derivada del perfil. Los motores quedan bloqueados en
// los tres perfiles vigentes (ver DRIVER_MOTORES_LISTO en domus_drivers.h).
// Hardware real de la feria: bomba (DRV canal A), Casa, Porche y Spare
// (2 LEDs azules de Jarvis en GPIO6). Cultivo NO tiene luz (índice 3 siempre
// sin etapa); no hay sensor de nivel de agua en el inventario.
// Bomba física en perfil 3 (S8050 directo) y perfil 4 (DRV8833 canal A).
constexpr bool SALIDA_FISICA_CASA[TOTAL_SALIDAS] = {
   PERFIL_CASA == PerfilCasa::BANCO_COMPLETO_S8050_IR ||
   PERFIL_CASA == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER,
   PERFIL_CASA != PerfilCasa::BANCO_SIN_ACTUADORES,
   PERFIL_CASA != PerfilCasa::BANCO_SIN_ACTUADORES,
   false,
   PERFIL_CASA != PerfilCasa::BANCO_SIN_ACTUADORES
};
// Bomba por GPIO directo (sin DRV): perfil 3 (S8050) y perfil 4 de feria
// (cableada al ESP32; el DRV queda fuera del camino de la bomba).
constexpr bool BOMBA_DIRECTA_S8050 =
   PERFIL_CASA == PerfilCasa::BANCO_COMPLETO_S8050_IR ||
   PERFIL_CASA == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER;
constexpr bool IR_CASA_HABILITADO =
   PERFIL_CASA >= PerfilCasa::BANCO_COMPLETO_S8050_IR;
static_assert(!(BOMBA_DIRECTA_S8050 && SALIDA_FISICA_CASA[3]),
               "Un solo S8050: bomba y ventilador no pueden habilitarse juntos");

// --- Luces directas por GPIO (sin 74HC595) ---

// --- DFPlayer Mini (perfil CASA_FINAL_DRV8833_DFPLAYER) ---
// SoftwareSerial en pines compartidos (nota 69 opcion c).
// RX y TX se asignan al inicio del setup() segun el perfil.
#define MP3_BUSY_PIN    -1
#define MP3_HABILITADO  (PERFIL_CASA == PerfilCasa::CASA_FINAL_DRV8833_DFPLAYER)

// ============================================================================
// SECCIÓN 2: CONSTANTES DE CALIBRACIÓN - AJUSTAR CON MEDICIONES REALES
// ============================================================================
// Cómo calibrar humedad: sube este firmware, abre el Monitor Serial, mete el
// sensor en tierra bien seca y anota el número en HUMEDAD_LECTURA_SECA, luego
// en tierra recién regada y anota el número en HUMEDAD_LECTURA_HUMEDA.
// El firmware convierte cualquier lectura entre esos dos extremos a un
// porcentaje 0-100% (0% = tan seco como tu medición seca, 100% = tan húmedo
// como tu medición húmeda). El sensor confirmado es resistivo; la polaridad
// exacta se determina con estas dos mediciones y la fórmula acepta ambos casos.
#define HUMEDAD_LECTURA_SECA     2800   // ADC crudo en tierra seca (0% humedad)
#define HUMEDAD_LECTURA_HUMEDA   1200   // ADC crudo en tierra recién regada (100%)
#define UMBRAL_HUMEDAD_SECA_PCT  35     // el riego automático se activa por debajo de este %
#define UMBRAL_HUMEDAD_HUMEDA_PCT 45    // y se apaga al alcanzar este %, evitando oscilaciones
// Riego AUTO por crudo (feria): seco ~4009 leído en banco; histéresis ancha
// para no oscilar con el sensor resistivo provisional.
#define UMBRAL_RIEGO_SEC_CRUDO   4000
#define UMBRAL_RIEGO_HUM_CRUDO   3000

#define HUMEDAD_MIN_VALIDA      50     // los rieles ADC se tratan como fallo
#define HUMEDAD_MAX_VALIDA      4045
// #define NIVEL_AGUA_* eliminados: sin sonda de depósito en el inventario.
// calibracion.nivelMinimo queda solo por compatibilidad del struct NVS.
#define PIR_RETENCION_MS              30000UL

// Calibración del LDR (fotoresistor), mismo principio que la humedad de
// tierra: dos lecturas de referencia se convierten a un 0-100% de luz.
// Para calibrar: abre el Monitor Serial, tapa el LDR con la mano (oscuridad)
// y anota el ADC crudo en LDR_LECTURA_OSCURO; luego ilumínalo con una
// linterna/luz directa y anota en LDR_LECTURA_BRILLANTE.
#define LDR_LECTURA_OSCURO      3200   // ADC crudo casi sin luz (0%)
#define LDR_LECTURA_BRILLANTE   400    // ADC crudo con luz directa (100%)
#define LDR_MIN_VALIDO          16
#define LDR_MAX_VALIDO          4079
#define UMBRAL_LUZ_OSCURO_PCT   25     // por debajo de este % se considera "oscuro" para automatización
#define UMBRAL_LUZ_CLARO_PCT    40     // histéresis: apagar solo al superar este valor

// DHT11: rango físico real del sensor (fuera de esto, se descarta la lectura)
#define DHT_TEMP_MIN_VALIDA_C    0.0
#define DHT_TEMP_MAX_VALIDA_C    50.0
#define DHT_HUM_MIN_VALIDA_PCT   20.0
#define DHT_HUM_MAX_VALIDA_PCT   90.0
#define DHT_INTERVALO_LECTURA_MS 2500   // el DHT11 no soporta lecturas más rápidas que ~1s, se deja margen

// Ventilador automático por temperatura ambiental (además de su control
// manual por controles físicos/Serial/voz, igual que el riego automático)
#define UMBRAL_TEMP_ALTA_C       28.0   // a partir de esto, se enciende el ventilador solo
#define UMBRAL_TEMP_NORMAL_C     26.0   // se apaga al bajar hasta aquí (histéresis de 2 °C)

// Intervalo de refresco de pantalla y de verificación de riego automático
#define INTERVALO_PANTALLA_MS   1500
#define INTERVALO_RIEGO_MS      5000
#define TIEMPO_MAXIMO_BOMBA_MS  120000UL // corte de seguridad: 2 minutos continuos

// --- Watchdog de hardware ---
#define WATCHDOG_TIMEOUT_S      8

// --- Supervisor de salud y degradación segura ---
// El supervisor no reinicia por memoria baja: apaga cargas y mantiene Serial
// disponible para diagnóstico. Así se evita convertir una falta de memoria
// transitoria en un bucle de reinicios.
#define MEMORIA_LIBRE_CRITICA_BYTES       32768UL
#define MEMORIA_LIBRE_RECUPERACION_BYTES  65536UL
#define INTERVALO_SUPERVISOR_MS            2000UL
#define TIEMPO_ARRANQUE_ESTABLE_MS        60000UL
#define MAX_COMANDOS_POR_SEGUNDO             12

// --- Sistema de errores en memoria (buffer circular) ---
#define MAX_ERRORES_GUARDADOS   6
#define LONGITUD_MAX_ERROR      64
// Un sensor caído registraba ~20 errores/s (86k acumulados en banco e
// inundaba el Serial). Cada origen avisa como máximo una vez cada 30 s;
// el contador fallosConsecutivos* sigue detectando a la 3ª lectura mala.
#define INTERVALO_AVISO_SENSOR_MS 30000UL

// --- I2C: reintentos ante fallo transitorio de pantalla ---
#define I2C_MAX_REINTENTOS      3
#define I2C_ESPERA_REINTENTO_MS 150
#define I2C_TIMEOUT_MS 10

// --- Lector microSD SPI independiente (pines provisionales) ---
#define SD_SCK_PIN   38
#define SD_MISO_PIN  39
#define SD_MOSI_PIN  47
#define SD_CS_PIN    48

// Contrato de hardware comprobado también por el compilador. Incluye los
// pines activos y los reservados para módulos opcionales para impedir que una
// ampliación futura reutilice silenciosamente una señal ya ocupada.
constexpr int PINES_RESERVADOS_DOMUS[] = {
  MAPA_CASA.suelo, MAPA_CASA.nivel, MAPA_CASA.ldr,
  MAPA_CASA.bomba, MAPA_CASA.casa, MAPA_CASA.porche,
  MAPA_CASA.cultivo, MAPA_CASA.spare,
  MAPA_CASA.paro, MAPA_CASA.micOff, MAPA_CASA.ir, MAPA_CASA.demo,
  MAPA_CASA.scl, MAPA_CASA.dht, MAPA_CASA.sda,
  SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN
};

constexpr bool pinesDomusSonUnicos() {
  const size_t cantidad = sizeof(PINES_RESERVADOS_DOMUS) /
                          sizeof(PINES_RESERVADOS_DOMUS[0]);
  for (size_t i = 0; i < cantidad; ++i) {
    for (size_t j = i + 1; j < cantidad; ++j) {
      if (PINES_RESERVADOS_DOMUS[i] == PINES_RESERVADOS_DOMUS[j]) return false;
    }
  }
  return true;
}

static_assert(TOTAL_SALIDAS == 5, "DOMUS requiere exactamente cinco cargas");
static_assert(pinesDomusSonUnicos(), "Hay GPIO duplicados en el mapa DOMUS");
static_assert(UMBRAL_RIEGO_SEC_CRUDO > UMBRAL_RIEGO_HUM_CRUDO,
              "Histeresis de riego crudo: seco debe superar a humedo");
static_assert(UMBRAL_HUMEDAD_SECA_PCT < UMBRAL_HUMEDAD_HUMEDA_PCT,
              "Histeresis de suelo invertida");
static_assert(UMBRAL_LUZ_OSCURO_PCT < UMBRAL_LUZ_CLARO_PCT,
              "Histeresis de luz invertida");
static_assert(UMBRAL_TEMP_NORMAL_C < UMBRAL_TEMP_ALTA_C,
              "Histeresis de temperatura invertida");
static_assert(MEMORIA_LIBRE_CRITICA_BYTES < MEMORIA_LIBRE_RECUPERACION_BYTES,
              "La recuperacion debe exigir mas memoria que el modo seguro");

// ============================================================================
// SECCIÓN 3: OBJETOS GLOBALES
// ============================================================================
// DFPlayer por HardwareSerial con pin remapping (nota 69 opcion c).
// UART1: RX=GPIO17 (SDA del LCD quemado, liberado), TX=GPIO18 (demo/MODO;
// boton gateado con !MP3_HABILITADO). GPIO11 no existe en la placa.
HardwareSerial SerialMP3(1); // UART1 reservada
DFPlayerTransport transporteDFPlayer(SerialMP3);
JarvisAudio jarvisAudio(transporteDFPlayer);
SPIClass spiMicroSD(FSPI);
std::atomic<bool> microSdMontada{false};
std::atomic<uint32_t> sdDescartados{0};
std::atomic<uint32_t> sdErrores{0};
std::atomic<int> sdUltimaPrueba{0}; // 0 pendiente/no probada, 1 correcta, -1 fallo
QueueHandle_t colaSD = nullptr;
String bufferComandoSerial;
bool descartarComandoHastaNuevaLinea = false;
bool micHabilitado = true;
bool paroEmergenciaActivo = false;
bool modoSeguroActivo = false;
bool watchdogActivo = false;
CalibracionDomus calibracion = {1, HUMEDAD_LECTURA_SECA, HUMEDAD_LECTURA_HUMEDA,
  LDR_LECTURA_OSCURO, LDR_LECTURA_BRILLANTE, 600, 0};
CalibracionDomus calibracionPendiente = calibracion;
bool calibracionGuardada = false;
uint8_t direccionLcdActiva = 0;
char motivoModoSeguro[48] = "ninguno";
bool ultimoBotonDemo = HIGH;
unsigned long ultimoCambioBotonDemoMs = 0;
unsigned long memoriaLibreMinima = ULONG_MAX;
unsigned long ultimaRevisionSaludMs = 0;
unsigned long inicioVentanaComandosMs = 0;
uint8_t comandosEnVentana = 0;
bool contadorReiniciosEstabilizado = false;
RTC_DATA_ATTR uint8_t reiniciosCriticosConsecutivos = 0;

// ---- Pantalla: solo se crea el objeto del tipo que se detectó ----
enum TipoPantalla { PANTALLA_NINGUNA, PANTALLA_LCD };
TipoPantalla pantallaActiva = PANTALLA_NINGUNA;

LiquidCrystal_I2C* lcd = nullptr;

// Pantalla final 16x2: el objeto vive siempre; si el LCD no responde, el
// puntero queda nulo y la clase lo tolera sin detener el resto del sistema.
PantallaFinal pantallaFinal;
IRCasa::Receptor receptorIR;
int8_t teclaIrPendiente = -1;

// Estado de las cinco cargas físicas del diseño vigente.
bool estadoSalidas[TOTAL_SALIDAS] = {false, false, false, false, false};

// Toda fuente de control pasa por el mismo contrato. Esto evita que controles
// físicos, Serial, automatización y voz mantienen estados incompatibles.
enum PropietarioActuador {
  PROPIETARIO_NINGUNO,
  PROPIETARIO_MANUAL_ON,
  PROPIETARIO_MANUAL_OFF,
  PROPIETARIO_AUTOMATICO
};

PropietarioActuador propietarioSalidas[TOTAL_SALIDAS] = {
  PROPIETARIO_NINGUNO, PROPIETARIO_NINGUNO, PROPIETARIO_NINGUNO,
  PROPIETARIO_NINGUNO, PROPIETARIO_NINGUNO
};
unsigned long bombaEncendidaDesdeMs = 0;


// Últimas lecturas válidas de sensores (para no mostrar basura si un sensor
// falla momentáneamente)
int ultimaHumedadValida = -1;       // ADC crudo, humedad de TIERRA
bool ultimaPresenciaValida = false;
unsigned long ultimaPresenciaMs = 0;
int ultimoHumedadPctValido = -1;    // % calibrado, humedad de TIERRA
int ultimoLdrCrudoValido = -1;
int ultimoLuzPctValido = -1;        // % calibrado, luz ambiental (LDR)
float ultimaTempCValida = -1.0;     // °C, DHT11 (aire)
float ultimaHumAireValida = -1.0;   // %HR ambiental, DHT11 (aire)
unsigned long ultimaLecturaDhtMs = 0;

// ============================================================================
// SECCIÓN 3B: SISTEMA DE ERRORES (buffer circular, visible en pantalla)
// ============================================================================
struct RegistroError {
  char mensaje[LONGITUD_MAX_ERROR];
  unsigned long momentoMs;
  bool ocupado;
};

RegistroError bufferErrores[MAX_ERRORES_GUARDADOS];
int indiceErrorActual = 0;
unsigned long totalErroresAcumulados = 0; // contador histórico, no se resetea al sobreescribir

// Contadores DHT a nivel de fichero (visibles en DIAGNOSTICO, que se define
// antes que la sección de sensores; Arduino no adelanta globales). Antes
// eran static locales invisibles y la suspensión era un latch permanente.
uint8_t fallosDhtConsecutivos = 0;
bool dhtSuspendido = false;
unsigned long ultimoReintentoDhtMs = 0;
#define DHT_REINTENTO_SUSPENDIDO_MS 60000UL

void log(const char* etiqueta, const char* mensaje);
void emitirEventoLocal(const char* linea);
bool leerHumedad(int &crudoSalida, int &pctSalida);
bool leerLuz(int &crudoSalida, int &pctSalida);
void registrarError(const String &origen, const String &mensaje);
String obtenerUltimoError();

void registrarError(const String &origen, const String &mensaje) {
  String completo = origen + ": " + mensaje;
  strncpy(bufferErrores[indiceErrorActual].mensaje, completo.c_str(), LONGITUD_MAX_ERROR - 1);
  bufferErrores[indiceErrorActual].mensaje[LONGITUD_MAX_ERROR - 1] = '\0';
  bufferErrores[indiceErrorActual].momentoMs = millis();
  bufferErrores[indiceErrorActual].ocupado = true;
  indiceErrorActual = (indiceErrorActual + 1) % MAX_ERRORES_GUARDADOS;
  totalErroresAcumulados++;
  log("ERROR", completo);
}

String obtenerUltimoError() {
  int idx = (indiceErrorActual - 1 + MAX_ERRORES_GUARDADOS) % MAX_ERRORES_GUARDADOS;
  if (!bufferErrores[idx].ocupado) return "";
  return String(bufferErrores[idx].mensaje);
}

// Constructor sin heap: los reportes se armaban con ~25 concatenaciones
// String (una reserva por +=). Ahora un solo snprintf a buffer estático;
// el String de retorno es una única asignación para el Serial.
String construirReporteDiagnostico() {
  static char buf[1024];
  const String ultimo = obtenerUltimoError();
  int humCrudo = -1, sueloCrudo = -1, ldrCrudo = -1;
  if (ultimaHumedadValida >= 0) humCrudo = (int)ultimaHumedadValida;
  if (ultimoHumedadPctValido >= 0) sueloCrudo = (int)ultimoHumedadPctValido;
  if (ultimoLdrCrudoValido >= 0) ldrCrudo = (int)ultimoLdrCrudoValido;
  snprintf(buf, sizeof(buf),
    "DIAGNOSTICO;"
    "PERFIL_CANDIDATO=%s;"
    "UPTIME_S=%lu;"
    "MEM_LIBRE=%lu;"
    "RAM_INTERNA=%u;RAM_INTERNA_MIN=%u;RAM_BLOQUE_MAX=%u;"
    "PSRAM_LIBRE=%u;PSRAM_MIN=%u;PSRAM_BLOQUE_MAX=%u;"
    "ERRORES_TOTAL=%lu;"
    "DHT_FALLOS=%u;DHT_SUSPENDIDO=%d;"
    "ULTIMO_ERROR=%s;"
    "PANTALLA=%s;"
    "MEM_MIN=%lu;"
    "MODO_SEGURO=%s;MOTIVO_SEGURO=%s;"
    "REINICIOS_CRITICOS=%d;"
    "WATCHDOG=%s;"
    "CALIBRACION=%s;"
    "IR=%s;IR_ULTIMO=0x%X;"
    "BOMBA_ETAPA=%s;"
    "SD_DESCARTADOS=%lu;SD_ERRORES=%lu;SD_PRUEBA=%d;"
    "RECONOCIMIENTO_VOZ=NO_USADO;AUDIO=APLAZADO;"
    "ADC_SUELRO=%d;ADC_LDR=%d;"
    "CAL_SECO=%d;CAL_HUMEDO=%d;CAL_OSCURO=%d;CAL_CLARO=%d;",
    nombrePerfilCasa(PERFIL_CASA),
    (unsigned long)(millis() / 1000),
    (unsigned long)esp_get_free_heap_size(),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
    (unsigned long)totalErroresAcumulados,
    (unsigned)fallosDhtConsecutivos, dhtSuspendido ? 1 : 0,
    ultimo.length() > 0 ? ultimo.c_str() : "ninguno",
    pantallaActiva == PANTALLA_LCD ? "LCD" : "NINGUNA",
    (unsigned long)(memoriaLibreMinima == ULONG_MAX ? 0 : memoriaLibreMinima),
    modoSeguroActivo ? "ON" : "OFF", motivoModoSeguro,
    reiniciosCriticosConsecutivos,
    watchdogActivo ? "ON" : "FALLO",
    calibracionGuardada ? "NVS" : "PROVISIONAL",
    IR_CASA_HABILITADO ? "ON" : "OFF",
    (unsigned)receptorIR.ultimoCodigo(),
    BOMBA_DIRECTA_S8050 ? "GPIO_DIRECTO" : "DRV",
    (unsigned long)sdDescartados.load(),
    (unsigned long)sdErrores.load(),
    (int)sdUltimaPrueba.load(),
    sueloCrudo, ldrCrudo,
    calibracion.sueloSeco, calibracion.sueloHumedo,
    calibracion.luzOscura, calibracion.luzClara);
  return String(buf);
}

// ============================================================================
// Dispatch de motores DRV8833 (perfil 4)
// ============================================================================
bool driverMotoresAplicarFinal(uint8_t canal, bool activar) {
  if (!DRIVER_MOTORES_LISTO) return false;
  if (canal > 1) return false;
  if (canal == 0) {
    // Bomba: AIN1=GPIO4, AIN2=GPIO7.
    // ON  = AIN1 HIGH, AIN2 LOW  -> forward
    // OFF = AIN1 LOW,  AIN2 LOW  -> coast (nunca reverse)
    digitalWrite(DRV8833_PIN_AIN1, activar ? HIGH : LOW);
    digitalWrite(DRV8833_PIN_AIN2, LOW);
  } else {
    // Canal 1: ventilador eliminado, sin accion
    return false;
  }
  return true;
}

// ============================================================================
// SECCIÓN 5: UTILIDAD DE LOG CON TIMESTAMP
// ============================================================================
void log(const char* etiqueta, const char* mensaje) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("ms][");
  Serial.print(etiqueta);
  Serial.print("] ");
  Serial.println(mensaje);
}
inline void log(const String &etiqueta, const String &mensaje) {
  log(etiqueta.c_str(), mensaje.c_str());
}
inline void log(const char* etiqueta, const String &mensaje) {
  log(etiqueta, mensaje.c_str());
}
inline void log(const String &etiqueta, const char* mensaje) {
  log(etiqueta.c_str(), mensaje);
}

// ============================================================================
// SECCIÓN 5B: ALMACENAMIENTO MICROSD LOCAL Y HISTORIAL
// ============================================================================
#define MAX_REGISTROS_HISTORIAL 64

enum class TipoRegistroHistorial {
  RIEGO_AUTO, RIEGO_MANUAL, LUZ_ENCENDIDA, LUZ_APAGADA,
  VENT_ENCENDIDO, VENT_APAGADO, PARO_EMERGENCIA, MODO_SEGURO,
  SENSOR_TEMP, SENSOR_HUMEDAD, SENSOR_LDR, SENSOR_PIR,
  SENSOR_NIVEL, CONFIGURACION, DIAGNOSTICO, DEMO_SECUENCIA
};

struct RegistroHistorial {
  TipoRegistroHistorial tipo;
  uint8_t indice;
  bool valor;
  float datoAdicional;
  unsigned long momento;
};

std::atomic<uint32_t> sdRegistrosHistorial{0};
std::atomic<uint16_t> indiceHistorial {0};
RegistroHistorial bufferHistorial[MAX_REGISTROS_HISTORIAL];

void registrarHistorial(TipoRegistroHistorial tipo, uint8_t indice, bool valor, float datoAdicional = 0) {
  if (!microSdMontada || colaSD == nullptr) return;
  const uint16_t idx = (uint16_t)(indiceHistorial.load() % MAX_REGISTROS_HISTORIAL);
  indiceHistorial.store((uint16_t)(idx + 1));
  RegistroHistorial &reg = bufferHistorial[idx];
  reg.tipo = tipo;
  reg.indice = indice;
  reg.valor = valor;
  reg.datoAdicional = datoAdicional;
  reg.momento = millis();

  TrabajoSD trabajo = {};
  trabajo.prueba = false;
  trabajo.momento = reg.momento;
  snprintf(trabajo.linea, sizeof(trabajo.linea), "%u;%u;%d;%.2f;%lu",
           (unsigned)tipo, (unsigned)indice, valor ? 1 : 0,
           (double)datoAdicional, (unsigned long)reg.momento);
  // Usamos la cola existente para logging SD (sin bloquear el control).
  if (xQueueSend(colaSD, &trabajo, 0) != pdTRUE) {
    sdDescartados++;
  }
}

// Registros de estadísticas acumuladas. Struct plano (trivially copyable):
// el lazo principal es el único escritor, la SD y el LCD solo leen copias.
struct EstadisticasDOMUS {
  uint32_t totalEncendidos = 0;
  uint32_t totalApagados = 0;
  uint32_t totalRiiegosAutomaticos = 0;
  uint32_t totalVentAutomaticos = 0;
  uint32_t totalCambiosLuz = 0;
  uint32_t totalEmergencias = 0;
  uint32_t totalErroresSensores = 0;
  unsigned long tiempoEncendidoBomba = 0;
  unsigned long tiempoEncendidoVentilador = 0;
};

EstadisticasDOMUS estadisticas;

inline void incrementarTotalEncendidos() { estadisticas.totalEncendidos++; }
inline void incrementarTotalApagados() { estadisticas.totalApagados++; }
inline void incrementarTotalRiiegosAutomaticos() { estadisticas.totalRiiegosAutomaticos++; }
inline void incrementarTotalVentAutomaticos() { estadisticas.totalVentAutomaticos++; }
inline void incrementarTotalCambiosLuz() { estadisticas.totalCambiosLuz++; }
inline void incrementarTotalEmergencias() { estadisticas.totalEmergencias++; }
inline void incrementarTotalErroresSensores() { estadisticas.totalErroresSensores++; }

// ============================================================================
// Función para emitir eventos de historial por Serial
// ============================================================================
// ============================================================================
// SECCIÓN 5C: EVENTOS DE HISTORIAL ESPECÍFICOS
// ============================================================================

bool probarMicroSD() {
  if (!microSdMontada) return false;
  const char* ruta = "/domus_selftest.txt";
  const char* marca = "PROJECT_DOMUS_SD_OK";

  SD.remove(ruta);
  File salida = SD.open(ruta, FILE_WRITE);
  if (!salida) return false;
  bool escrita = salida.println(marca) == strlen(marca) + 2;
  salida.close();
  if (!escrita) return false;

  File entrada = SD.open(ruta, FILE_READ);
  if (!entrada) return false;
  String contenido = entrada.readStringUntil('\n');
  entrada.close();
  contenido.trim();
  return contenido == marca;
}

// Solo esta tarea posee el bus SD: ninguna orden de control espera a la tarjeta.
void tareaMicroSD(void*) {
  spiMicroSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  microSdMontada = SD.begin(SD_CS_PIN, spiMicroSD, 4000000U);
  sdUltimaPrueba = probarMicroSD() ? 1 : -1;
  if (sdUltimaPrueba != 1) {
    microSdMontada = false;
    sdErrores++;
  }
  TrabajoSD trabajo;
  while (microSdMontada && xQueueReceive(colaSD, &trabajo, portMAX_DELAY) == pdTRUE) {
    bool ok = true;
    if (trabajo.prueba) {
      ok = probarMicroSD();
      sdUltimaPrueba = ok ? 1 : -1;
    } else {
      File archivo = SD.open("/domus.log", FILE_APPEND);
      ok = bool(archivo);
      if (ok && archivo.size() + sizeof(trabajo.linea) + 16 > 256UL * 1024) {
        archivo.close();
        ok = !SD.exists("/domus.prev.log") || SD.remove("/domus.prev.log");
        if (ok) ok = SD.rename("/domus.log", "/domus.prev.log");
        if (ok) { archivo = SD.open("/domus.log", FILE_APPEND); ok = bool(archivo); }
      }
      if (ok) {
        char registro[216];
        int longitud = snprintf(registro, sizeof(registro), "%lu;%s\n",
                                (unsigned long)trabajo.momento, trabajo.linea);
        ok = longitud > 0 && longitud < (int)sizeof(registro) &&
          archivo.write((const uint8_t*)registro, longitud) == (size_t)longitud;
      }
      archivo.close();
    }
    if (!ok) { sdErrores++; microSdMontada = false; }
  }
  SD.end();
  vTaskDelete(NULL);
}

bool solicitarPruebaSD() {
  if (!microSdMontada || colaSD == nullptr) return false;
  TrabajoSD trabajo = {};
  trabajo.prueba = true;
  return xQueueSend(colaSD, &trabajo, 0) == pdTRUE;
}

void inicializarMicroSD() {
  if (!MICROSD_HABILITADA) return;
  colaSD = xQueueCreate(8, sizeof(TrabajoSD));
  if (colaSD == nullptr || xTaskCreate(tareaMicroSD, "domus-sd", 6144, NULL, 1, NULL) != pdPASS) {
    if (colaSD != nullptr) { vQueueDelete(colaSD); colaSD = nullptr; }
    sdErrores++;
    registrarError("SD", "Sin recursos; registro desactivado");
  }
}

// ============================================================================
// SECCIÓN 6: DETECCIÓN DEL LCD1602 (I2C SCAN)
// ============================================================================
bool escanearBusI2C(bool &hayLcd, uint8_t &dirLcd) {
  hayLcd = false;
  int dispositivosEncontrados = 0;
  uint8_t direccionUnica = 0;
  uint8_t direccionPreferida = 0;

  for (uint8_t dir = 0x08; dir <= 0x77; ++dir) {
    Wire.beginTransmission(dir);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      dispositivosEncontrados++;
      direccionUnica = dir;
      log("I2C", "Dispositivo encontrado en 0x" + String(dir, HEX));
      if (dir == DIR_LCD_1 || dir == DIR_LCD_2) direccionPreferida = dir;
    }
  }
  dirLcd = direccionPreferida != 0 ? direccionPreferida :
           (dispositivosEncontrados == 1 ? direccionUnica : 0);
  hayLcd = dirLcd != 0;
  if (dispositivosEncontrados > 1 && direccionPreferida == 0)
    log("I2C", "Direccion LCD ambigua: desconecta otros dispositivos I2C");
  return dispositivosEncontrados > 0;
}

// LCD1602 quemado y descartado (nota 80): no se inicia I2C; libera
// GPIO17/13 para el UART del DFPlayer y evita errores de bus.
constexpr bool LCD_DESCARTADO = true;

void detectarPantalla() {
  if (LCD_DESCARTADO) {
    log("PANTALLA", "LCD descartado (quemado); I2C no iniciado, modo headless");
    pantallaActiva = PANTALLA_NINGUNA;
    return;
  }
  if (!Wire.begin(MAPA_CASA.sda, MAPA_CASA.scl)) {
    registrarError("I2C", "No se pudo iniciar; sin pantalla");
    return;
  }
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  // Banco en protoboard con cables jumper: 50 kHz reduce flancos y errores
  // observados en el LCD/PCF8574 sin afectar su velocidad de refresco.
  Wire.setClock(50000);
  delay(50);

  bool hayLcd = false;
  uint8_t dirLcdEncontrada = 0;
  bool busRespondio = false;

  for (int intento = 1; intento <= I2C_MAX_REINTENTOS && !busRespondio; intento++) {
    busRespondio = escanearBusI2C(hayLcd, dirLcdEncontrada);
    if (!busRespondio) {
      log("I2C", "Intento " + String(intento) + "/" + String(I2C_MAX_REINTENTOS) + ": bus I2C sin respuesta, reintentando...");
      delay(I2C_ESPERA_REINTENTO_MS);
    }
  }

  if (!busRespondio) {
    registrarError("I2C", "Bus sin ningun dispositivo tras " + String(I2C_MAX_REINTENTOS) + " intentos. Revisar cableado SDA/SCL.");
  }

  if (hayLcd) {
    lcd = new (std::nothrow) LiquidCrystal_I2C(dirLcdEncontrada, 16, 2);
    if (lcd == nullptr) {
      registrarError("LCD", "Sin memoria; sin pantalla");
      return;
    }
    direccionLcdActiva = dirLcdEncontrada;
    lcd->init();
    lcd->backlight();
    pantallaActiva = PANTALLA_LCD;
    log("PANTALLA", "LCD 1602 detectado en 0x" + String(dirLcdEncontrada, HEX));
  } else {
    pantallaActiva = PANTALLA_NINGUNA;
    log("PANTALLA", "Ninguna pantalla detectada, sistema seguirá en modo headless");
  }
}

bool pantallaDisponible() {
  if (pantallaActiva != PANTALLA_LCD || lcd == nullptr) return false;
  Wire.beginTransmission(direccionLcdActiva);
  if (Wire.endTransmission() == 0) return true;
  pantallaActiva = PANTALLA_NINGUNA;
  registrarError("LCD", "Desconectada; control sigue activo");
  return false;
}

// ---- Pantalla final (domus_pantalla.h, clase PantallaFinal) ----
// LCD descartado (quemado): la clase queda integrada para un futuro panel,
// pero el .ino no refresca ni navega vistas. El escaneo I2C y su registro
// Serial de arriba se conservan intactos.

// ============================================================================
// SECCIÓN 7: MÓDULO MP3 (respuestas habladas, opcional)
// ============================================================================
// variante: 0 = aleatoria; 1-4 = pista fija (1 = ON manual, 2 = OFF manual,
// 3 = ON automático, 4 = OFF automático en carpetas de conmutador).
bool anunciarJarvis(EventoJarvis evento, bool alertaAutomatica = false,
                    uint8_t variante = 0) {
  if (!MP3_HABILITADO) return false;
  const bool reproducida =
      jarvisAudio.reproducir(evento, millis(), alertaAutomatica, variante);
  if (reproducida) {
    char detalle[32];
    snprintf(detalle, sizeof(detalle), "Carpeta %u pista %u",
             (unsigned)JarvisAudio::carpetaPara(evento, jarvisAudio.vozActual()),
             (unsigned)variante);
    log("MP3", detalle);
  }
  return reproducida;
}

// Grupo aleatorio entre dos variantes (p. ej. 1-2 = par manual).
bool anunciarJarvisGrupo(EventoJarvis evento, bool alertaAutomatica,
                         uint8_t vA, uint8_t vB) {
  if (!MP3_HABILITADO) return false;
  return jarvisAudio.reproducirGrupo(evento, millis(), alertaAutomatica, vA, vB);
}

// Carpeta del botón que gobierna cada salida: bomba = tecla 5,
// casa = tecla 1, porche = tecla 2, cultivo = tecla 3, spare = tecla CH.
EventoJarvis carpetaSalida(int indice) {
  switch (indice) {
    case 0: return EventoJarvis::TECLA_5;
    case 1: return EventoJarvis::TECLA_1;
    case 2: return EventoJarvis::TECLA_2;
    case 3: return EventoJarvis::TECLA_3;
    case 4: return EventoJarvis::CH;
    default: return EventoJarvis::TECLA_9;
  }
}

// ============================================================================
// SECCIÓN 8: CONTROL DE RELÉS
// ============================================================================
// Contador de fallos de verificación por relé.
int fallosVerificacionSalida[TOTAL_SALIDAS] = {0, 0, 0, 0, 0};
#define MAX_FALLOS_ANTES_DE_ALERTA_PERSISTENTE 3

// IMPORTANTE (corrección del feedback #5 de v3): esta función NO verifica
// físicamente que el relé conmutó, ni que la carga real cambió de estado.
// Verifica únicamente que el GPIO del ESP32 quedó en el nivel lógico que
// se le acaba de ordenar. Un cable suelto entre el ESP32 y el módulo de
// relés, o un relé dañado, pueden hacer que esto devuelva true aunque nada
// haya cambiado físicamente. Para detectar el estado físico real del relé
// haría falta una señal de realimentación por hardware (ej. leer el propio
// contacto NO/NC del relé hacia un pin de entrada), que este kit no tiene.
int nivelSalida(int indice, bool encendida) {
  return encendida == SALIDA_ACTIVA_EN_BAJO[indice] ? LOW : HIGH;
}

bool verificarNivelLogicoSalida(int indice, bool estadoEsperado) {
  int nivelEsperado = nivelSalida(indice, estadoEsperado);
  int nivelReal = digitalRead(MAPA_CASA.salidas[indice]);
  return nivelReal == nivelEsperado;
}

// Devuelve true si el nivel lógico solicitado se leyó de vuelta en el GPIO. Los
// llamadores (Serial, botones y voz) usan este valor de retorno para construir un
// ACK o NACK del controlador. Esto no confirma el movimiento de un actuador:
// esa evidencia requiere realimentación física que el montaje aún no incorpora.
bool solicitarSalida(int indice, bool anunciarPorVoz = true) {
  if (indice < 0 || indice >= TOTAL_SALIDAS) {
    registrarError("SALIDA", "Indice invalido solicitado: " + String(indice));
    return false;
  }
  if (estadoSalidas[indice]) return true; // ya encendido, se considera éxito idempotente

  pinMode(MAPA_CASA.salidas[indice], OUTPUT);
  digitalWrite(MAPA_CASA.salidas[indice], nivelSalida(indice, true));

  if (!verificarNivelLogicoSalida(indice, true)) {
    fallosVerificacionSalida[indice]++;
    registrarError("SALIDA", String(NOMBRES_SALIDAS[indice]) + " no confirmo nivel de encendido en GPIO");
    if (fallosVerificacionSalida[indice] >= MAX_FALLOS_ANTES_DE_ALERTA_PERSISTENTE) {
      registrarError("SALIDA", String(NOMBRES_SALIDAS[indice]) + " fallo logico repetido, revisar GPIO");
    }
    return false;
  }

fallosVerificacionSalida[indice] = 0;
  estadoSalidas[indice] = true;
  log("SALIDA", String(NOMBRES_SALIDAS[indice]) + " -> ENCENDIDO (nivel GPIO verificado)");
  incrementarTotalEncendidos();
  registrarHistorial(TipoRegistroHistorial::LUZ_ENCENDIDA, (uint8_t)indice, true);
  if (anunciarPorVoz && MP3_HABILITADO) {
    anunciarJarvis(carpetaSalida(indice), false, 1);
  }
  return true;
}

bool desactivarSalida(int indice, bool anunciarPorVoz = true) {
  if (indice < 0 || indice >= TOTAL_SALIDAS) {
    registrarError("SALIDA", "Indice invalido solicitado: " + String(indice));
    return false;
  }
  if (!estadoSalidas[indice]) return true; // ya apagado, éxito idempotente

  pinMode(MAPA_CASA.salidas[indice], OUTPUT);
  digitalWrite(MAPA_CASA.salidas[indice], nivelSalida(indice, false));

  if (!verificarNivelLogicoSalida(indice, false)) {
    fallosVerificacionSalida[indice]++;
    registrarError("SALIDA", String(NOMBRES_SALIDAS[indice]) + " no confirmo nivel de apagado en GPIO");
    return false;
  }

fallosVerificacionSalida[indice] = 0;
  estadoSalidas[indice] = false;
  log("SALIDA", String(NOMBRES_SALIDAS[indice]) + " -> APAGADO (nivel GPIO verificado)");
  incrementarTotalApagados();
  registrarHistorial(TipoRegistroHistorial::LUZ_APAGADA, (uint8_t)indice, false);
  if (anunciarPorVoz && MP3_HABILITADO) {
    anunciarJarvis(carpetaSalida(indice), false, 2);
  }
  return true;
}

String construirRespuestaJarvis(const OrdenActuador &orden, bool estadoAnterior,
                                const ResultadoOrden &resultado) {
  if (!resultado.exito) {
    if (String(resultado.motivo) == "confianza_baja") return "No entendi la orden.";
    return "No pude completar la orden. Revisa el dispositivo.";
  }

  const String nombre = String(NOMBRES_SALIDAS[orden.indiceRele]);
  if (estadoAnterior == orden.encender) {
    return orden.encender
      ? "El dispositivo " + nombre + " ya estaba encendido."
      : "El dispositivo " + nombre + " ya estaba apagado.";
  }
  return orden.encender
    ? "He encendido " + nombre + "."
    : "He apagado " + nombre + ".";
}

// Punto único de salida de Jarvis. Hoy deja la frase observable en Serial;
// más adelante podrá disparar frases grabadas sin añadir reconocimiento.
void responderJarvis(const String &texto) {
  log("JARVIS_TEXTO", texto);
}

ResultadoOrden ejecutarOrdenActuador(const OrdenActuador &orden) {
  if (orden.indiceRele < 0 || orden.indiceRele >= TOTAL_SALIDAS) {
    registrarError("ORDEN", "Indice de rele fuera de rango");
    return {false, false, "indice_invalido"};
  }

  if (paroEmergenciaActivo && orden.encender) {
    log("SEGURIDAD", "Encendido rechazado: paro de emergencia activo");
    return {false, false, "paro_emergencia"};
  }

  if (modoSeguroActivo && orden.encender) {
    log("SEGURIDAD", "Encendido rechazado: modo seguro activo");
    return {false, false, "modo_seguro"};
  }

  // Bomba por driver (F1): el motivo reportado distingue "sin driver
  // validado" de "sin etapa instalada". Cultivo/canal B no existen.
  if (orden.encender && orden.indiceRele == 0 && !BOMBA_DIRECTA_S8050 &&
      !driverMotoresListo()) {
    log("SEGURIDAD", "Encendido rechazado: driver de motores no validado (F1)");
    if (orden.origen == ORIGEN_IR || orden.origen == ORIGEN_MANUAL) {
      responderJarvis("No funciono.");
      anunciarJarvis(EventoJarvis::FALLO, false, 1);
    }
    return {false, false, "driver_no_listo"};
  }

  // Habilitación física del perfil: ninguna fuente enciende una salida sin
  // etapa instalada (nota 53/54/55). Cultivo no tiene luz en el hardware.
  if (orden.encender && !SALIDA_FISICA_CASA[orden.indiceRele]) {
    log("SEGURIDAD", "Encendido rechazado: salida sin etapa en este perfil");
    if (orden.origen == ORIGEN_IR || orden.origen == ORIGEN_MANUAL) {
      responderJarvis("No funciono.");
      anunciarJarvis(EventoJarvis::FALLO, false, 1);
    }
    return {false, false, "salida_no_instalada"};
  }

  const bool estadoAnterior = estadoSalidas[orden.indiceRele];
  const bool exito = orden.encender
    ? solicitarSalida(orden.indiceRele, false)
    : desactivarSalida(orden.indiceRele, false);
  if (!exito) {
    ResultadoOrden fallo = {false, false, "gpio_no_confirmado"};
    if (orden.origen == ORIGEN_IR || orden.origen == ORIGEN_MANUAL) {
      responderJarvis("No funciono.");
      anunciarJarvis(EventoJarvis::FALLO, false, 1);
    }
    return fallo;
  }

  // Perfil final: conducir DRV8833 solo en la bomba (canal A).
  if (orden.indiceRele == 0 && driverMotoresListo()) {
    driverMotoresAplicarFinal(0, orden.encender);
  }

  // Una orden manual/IR toma propiedad incluso si fue idempotente.
  // Así el automático no apagará después algo que el usuario decidió dejar ON.
  if (orden.encender) {
    propietarioSalidas[orden.indiceRele] = (orden.origen == ORIGEN_AUTOMATICO)
      ? PROPIETARIO_AUTOMATICO : PROPIETARIO_MANUAL_ON;
    if (orden.indiceRele == 0 && !estadoAnterior) bombaEncendidaDesdeMs = millis();
  } else {
    if (orden.origen == ORIGEN_AUTOMATICO) {
      propietarioSalidas[orden.indiceRele] = PROPIETARIO_NINGUNO;
    } else {
      // Un apagado manual, por IR o por seguridad persiste hasta recibir
      // explícitamente el comando *_AUTO (o hasta un reinicio controlado).
      propietarioSalidas[orden.indiceRele] = PROPIETARIO_MANUAL_OFF;
    }
    if (orden.indiceRele == 0) bombaEncendidaDesdeMs = 0;
  }

  log("ORDEN", String(orden.nombre ? orden.nombre : "SIN_NOMBRE") +
      ";origen=" + String((int)orden.origen) +
      ";estado=" + String(orden.encender ? 1 : 0));
  ResultadoOrden resultado = {true, estadoAnterior != orden.encender, "ok"};
  if (resultado.cambioReal) {
    // Los cortes del sistema (nivel, sensor, timeout) suenan como automáticos
    // (variantes 3/4 con cooldown), no como órdenes manuales.
    const bool automatica = orden.origen == ORIGEN_AUTOMATICO ||
        orden.origen == ORIGEN_SISTEMA;
    // Pista fija por botón: 1 = ON manual, 2 = OFF manual,
    // 3 = ON automático, 4 = OFF automático.
    uint8_t variante;
    if (orden.encender) variante = automatica ? 3 : 1;
    else variante = automatica ? 4 : 2;
    anunciarJarvis(carpetaSalida(orden.indiceRele), automatica, variante);
  }
  if (orden.origen == ORIGEN_IR) {
    responderJarvis(construirRespuestaJarvis(orden, estadoAnterior, resultado));
  }
  // Prioridad de la bomba sobre las luces: al encender riego, todo apagado.
  if (orden.encender && orden.indiceRele == 0 && estadoSalidas[0]) {
    for (int i = 1; i < TOTAL_SALIDAS; ++i) {
      if (i == 3 || !SALIDA_FISICA_CASA[i] || !estadoSalidas[i]) continue;
      OrdenActuador apague = {i, false, ORIGEN_SISTEMA, 1.0f, "BOMBA_ON_APAGA_LUCES"};
      ejecutarOrdenActuador(apague);
    }
  }
  return resultado;
}

void verificarLimiteBomba() {
  if (!estadoSalidas[0] || bombaEncendidaDesdeMs == 0) return;
  if (millis() - bombaEncendidaDesdeMs < TIEMPO_MAXIMO_BOMBA_MS) return;

  registrarError("SEGURIDAD", "Bomba detenida por tiempo maximo continuo");
  OrdenActuador corte = {0, false, ORIGEN_SISTEMA, 1.0f, "BOMBA_TIMEOUT"};
  ResultadoOrden resultado = ejecutarOrdenActuador(corte);
  if (resultado.exito) emitirEventoLocal("EVENTO;BOMBA_TIMEOUT;0");
}

// ============================================================================
// SECCIÓN 9: LECTURA Y VALIDACIÓN DE SENSORES
// ============================================================================
// 4 muestras bastan para sensores lentos (suelo/nivel/LDR): 8 duplicaba el
// tiempo bloqueado por vuelta sin mejorar la lectura. El test nativo
// sustituye esta función por un fake, así que el valor es libre.
int leerSensorPromediado(int pin, int muestras = 4) {
  long suma = 0;
  for (int i = 0; i < muestras; i++) {
    suma += analogRead(pin);
    delayMicroseconds(100);
  }
  const int promedio = muestras > 0 ? (int)(suma / muestras) : 0;
  // Detección de pin flotante: un pin sin nada conectado sigue a los
  // pull-up/down internos hasta los rieles; un sensor real (divisor de
  // baja impedancia) apenas se mueve. Devuelve -1 = desconectado, que
  // cae fuera de todos los rangos válidos.
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(200);
  const int conPullUp = analogRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(200);
  const int conPullDown = analogRead(pin);
  pinMode(pin, INPUT);
  if (conPullUp > 3500 && conPullDown < 600) return -1;
  return promedio;
}

int fallosConsecutivosHumedad = 0;
#define MAX_FALLOS_ANTES_DE_REGISTRAR 3

// Conversor único ADC->%: los dos anteriores (humedad y LDR) eran el mismo
// cuerpo duplicado. Funciona sin importar si el extremo bajo es el número
// más alto o más bajo (soporta sensores de cualquier polaridad).
int convertirRangoAPorcentaje(int lecturaCruda, int extremoBajo, int extremoAlto) {
  long rango = (long)extremoAlto - (long)extremoBajo;
  if (rango == 0) return 0; // evita división por cero si no se calibró
  long pct = ((long)lecturaCruda - extremoBajo) * 100L / rango;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

int convertirHumedadAPorcentaje(int lecturaCruda) {
  return convertirRangoAPorcentaje(lecturaCruda, calibracion.sueloSeco,
                                   calibracion.sueloHumedo);
}

bool leerHumedad(int &crudoSalida, int &pctSalida) {
  int lectura = leerSensorPromediado(MAPA_CASA.suelo);
  if (lectura < HUMEDAD_MIN_VALIDA || lectura > HUMEDAD_MAX_VALIDA) {
    static unsigned long ultimoAvisoMs = 0;
    fallosConsecutivosHumedad++;
    if (fallosConsecutivosHumedad >= MAX_FALLOS_ANTES_DE_REGISTRAR) {
      if (millis() - ultimoAvisoMs >= INTERVALO_AVISO_SENSOR_MS) {
        ultimoAvisoMs = millis();
        registrarError("SENSOR", "Humedad fuera de rango, revisar conexion");
      }
      fallosConsecutivosHumedad = 0;
    }
    return false;
  }
  fallosConsecutivosHumedad = 0;
  crudoSalida = lectura;
  pctSalida = convertirHumedadAPorcentaje(lectura);
  ultimaHumedadValida = lectura;
  ultimoHumedadPctValido = pctSalida;
  return true;
}

// Sin sensor de nivel de agua en el inventario de hardware: no se lee GPIO16
// ni se usa como interlock. La bomba se gobierna por humedad de suelo y
// TIEMPO_MAXIMO_BOMBA_MS.

int fallosConsecutivosLdr = 0;

bool leerLuz(int &crudoSalida, int &pctSalida) {
  int lectura = leerSensorPromediado(MAPA_CASA.ldr);
  if (lectura < LDR_MIN_VALIDO || lectura > LDR_MAX_VALIDO) {
    static unsigned long ultimoAvisoLdrMs = 0;
    fallosConsecutivosLdr++;
    if (fallosConsecutivosLdr >= MAX_FALLOS_ANTES_DE_REGISTRAR) {
      if (millis() - ultimoAvisoLdrMs >= INTERVALO_AVISO_SENSOR_MS) {
        ultimoAvisoLdrMs = millis();
        registrarError("SENSOR", "LDR fuera de rango, revisar divisor");
      }
      fallosConsecutivosLdr = 0;
    }
    return false;
  }
  fallosConsecutivosLdr = 0;
  crudoSalida = lectura;
  pctSalida = convertirRangoAPorcentaje(lectura, calibracion.luzOscura,
                                        calibracion.luzClara);
  ultimoLdrCrudoValido = lectura;
  ultimoLuzPctValido = pctSalida;
  return true;
}

// ============================================================================
// SECCIÓN 11: COMANDOS LOCALES POR USB SERIAL
// ============================================================================
// Comandos de texto terminados en salto de línea:
//   RIEGO_ON / RIEGO_OFF | LUZ1_ON / LUZ1_OFF | LUZ2_ON / LUZ2_OFF
//   SPARE_ON / SPARE_OFF | ESTADO | DIAGNOSTICO
//
//   "ACK;<comando>;<estado_logico_0_o_1>"   -> el comando se aplicó y se confirmó por GPIO
//   "NACK;<comando>;<motivo>"               -> el comando no pudo confirmarse o fue rechazado

String construirReporteEstado() {
  static char buf[320];
  snprintf(buf, sizeof(buf),
    "ESTADO;%s=%d;%s=%d;%s=%d;%s=%d;%s=%d;"
    "HUM=%d;HUM_PCT=%d;PRESENCIA=1;"
    "TEMP_C=%.1f;HUM_AIRE_PCT=%.1f;LUZ_PCT=%d;"
    "MIC=%s;SD=%s;EMERGENCIA=%s;MODO_SEGURO=%s;",
    NOMBRES_SALIDAS[0], estadoSalidas[0] ? 1 : 0,
    NOMBRES_SALIDAS[1], estadoSalidas[1] ? 1 : 0,
    NOMBRES_SALIDAS[2], estadoSalidas[2] ? 1 : 0,
    NOMBRES_SALIDAS[3], estadoSalidas[3] ? 1 : 0,
    NOMBRES_SALIDAS[4], estadoSalidas[4] ? 1 : 0,
    ultimaHumedadValida, ultimoHumedadPctValido,
    (double)ultimaTempCValida, (double)ultimaHumAireValida,
    ultimoLuzPctValido,
    micHabilitado ? "ON" : "OFF",
    microSdMontada.load() ? "ON" : "OFF",
    paroEmergenciaActivo ? "ON" : "OFF",
    modoSeguroActivo ? "ON" : "OFF");
  return String(buf);
}

void emitirPruebaGuiada() {
  emitirEventoLocal("PRUEBA;INICIO;COPIA_HASTA_FIN");
  emitirEventoLocal("PRUEBA;LCD=" + String(pantallaActiva == PANTALLA_LCD ? 1 : 0) +
    ";DIR=0x" + String(direccionLcdActiva, HEX) +
    ";IR=" + String(IR_CASA_HABILITADO ? 1 : 0) +
    ";IR_ULTIMO=0x" + String(receptorIR.ultimoCodigo(), HEX));
  emitirEventoLocal(construirReporteEstado());
  emitirEventoLocal(construirReporteDiagnostico());
  emitirEventoLocal("PRUEBA;ACCIONES=TAPA_LDR,PULSA_MODO,PRUEBA_IR,PULSA_STOP");
  emitirEventoLocal("PRUEBA;BOMBA=SUMERGIDA_Y_RIEGO_ON;AUTO=RIEGO_AUTO");
  emitirEventoLocal("PRUEBA;FIN");
}

const char* COMANDOS_VALIDOS[] = {
   "RIEGO_ON", "RIEGO_OFF", "LUZ1_ON", "LUZ1_OFF", "LUZ2_ON", "LUZ2_OFF",
   "RIEGO_AUTO", "LUZ1_AUTO", "LUZ2_AUTO",
   "SPARE_ON", "SPARE_OFF", "SPARE_AUTO",
   "TODO_ON", "TODO_OFF", "DEMO_ON", "DEMO_OFF",
   "ESTADO", "DIAGNOSTICO", "PRUEBA", "PARO", "REARMAR", "RECUPERAR",
   "MIC_ESTADO", "SD_PRUEBA", "PINTEST_ALL", "CAL_SUELDO"
};
const int CANTIDAD_COMANDOS_VALIDOS =
  sizeof(COMANDOS_VALIDOS) / sizeof(COMANDOS_VALIDOS[0]);

bool esComandoValido(const String &comando) {
  for (int i = 0; i < CANTIDAD_COMANDOS_VALIDOS; i++) {
    if (comando == COMANDOS_VALIDOS[i]) return true;
  }
  return false;
}

bool permitirComandoSerial() {
  const unsigned long ahora = millis();
  if (ahora - inicioVentanaComandosMs >= 1000UL) {
    inicioVentanaComandosMs = ahora;
    comandosEnVentana = 0;
  }
  if (comandosEnVentana >= MAX_COMANDOS_POR_SEGUNDO) return false;
  comandosEnVentana++;
  return true;
}

// Aplica un comando de control de relé y envía ACK/NACK real. Centraliza la
// lógica que antes estaba repetida por cada carga en procesarComandoBluetooth().
void ejecutarComandoRele(const String &comando, int indice, bool encender,
                         OrigenOrden origen = ORIGEN_MANUAL, float confianza = 1.0f) {
  OrdenActuador orden = {indice, encender, origen, confianza, comando.c_str()};
  ResultadoOrden resultado = ejecutarOrdenActuador(orden);
  if (resultado.exito) {
    emitirEventoLocal("ACK;" + comando + ";" + String(estadoSalidas[indice] ? 1 : 0));
  } else {
    emitirEventoLocal("NACK;" + comando + ";" + String(resultado.motivo));
  }
}

void alternarSalidaIR(int indice, const char* nombre, const char* etiqueta) {
  const bool pedido = !estadoSalidas[indice];
  String comando = String("IR_") + nombre + (pedido ? "_ON" : "_OFF");
  ejecutarComandoRele(comando, indice, pedido, ORIGEN_IR);
  // Confirmación en LCD con el estado realmente aplicado: el despachador
  // puede rechazar por PARO, nivel o driver; entonces se muestra BLOQ.
  char aviso[17];
  if (estadoSalidas[indice] == pedido) {
    snprintf(aviso, sizeof(aviso), "%s %s", etiqueta, pedido ? "ON" : "OFF");
  } else {
    snprintf(aviso, sizeof(aviso), "%s BLOQ", etiqueta);
  }
  pantallaFinal.mostrarMensaje(aviso, "Mando IR");
}

// Mapa de las 21 teclas CAR MP3 reordenado para la feria (sin LCD, sin
// audio): tope = controles usables (modos, luces, riego, demo), abajo =
// alias numéricos y consultas seriales al final. MANUAL deja las luces
// fijas; AUTO es invernadero (riego por suelo) con parpadeo solo de
// transición y luego luces apagadas.
enum ModoLucesIR : uint8_t { MODOLUCES_MANUAL, MODOLUCES_AUTO };
ModoLucesIR modoLucesIR = MODOLUCES_MANUAL;
unsigned long ultimaParpadeoMs = 0;
const unsigned long INTERVALO_PARPADEO_MS = 750UL;
// Transición MANUAL→AUTO: parpadea ~3 s y luego las luces quedan apagadas
// (AUTO es el invernadero, no un show de luces permanente).
unsigned long parpadeoTransicionHastaMs = 0;
bool autoTransicionLuces = false;
const unsigned long DURACION_TRANSICION_AUTO_MS = 3000UL;
enum EstadoDemo { DEMO_DETENIDO, DEMO_EN_CURSO };
EstadoDemo estadoDemo = DEMO_DETENIDO;
void iniciarSecuenciaDemo();
void detenerSecuenciaDemo();
void demoSecuenciaActualizar();
void parpadeoAutoActualizar();
void automatizarInvernadero();

void alternarTodasLucesIR(bool encender, const char* origen) {
  ejecutarComandoRele(String(origen) + (encender ? "_ON" : "_OFF"), 1, encender, ORIGEN_IR);
  ejecutarComandoRele(String(origen) + (encender ? "_ON" : "_OFF"), 2, encender, ORIGEN_IR);
  ejecutarComandoRele(String(origen) + (encender ? "_ON" : "_OFF"), 4, encender, ORIGEN_IR);
}

void fijarModoAutoIR() {
  modoLucesIR = MODOLUCES_AUTO;
  detenerSecuenciaDemo();
  ultimaParpadeoMs = millis();
  parpadeoTransicionHastaMs = millis() + DURACION_TRANSICION_AUTO_MS;
  autoTransicionLuces = true;
  for (int i = 0; i < TOTAL_SALIDAS; ++i) {
    propietarioSalidas[i] = PROPIETARIO_AUTOMATICO;
  }
  alternarTodasLucesIR(true, "IR_AUTO_LUCES");
  log("MODO", "Auto por IR: parpadeo de transicion y riego por suelo");
  emitirEventoLocal("ACK;IR;MODO_AUTO");
}

void fijarModoManualIR() {
  modoLucesIR = MODOLUCES_MANUAL;
  autoTransicionLuces = false;
  detenerSecuenciaDemo();
  for (int i = 0; i < TOTAL_SALIDAS; ++i) {
    propietarioSalidas[i] =
        estadoSalidas[i] ? PROPIETARIO_MANUAL_ON : PROPIETARIO_MANUAL_OFF;
  }
  log("MODO", "Manual por IR: luces fijas en su estado actual");
  emitirEventoLocal("ACK;IR;MODO_MANUAL");
}

// Transición de modo: parpadea unos segundos y apaga (no parpadeo eterno).
void parpadeoAutoActualizar() {
  if (modoLucesIR != MODOLUCES_AUTO) return;
  if (paroEmergenciaActivo || modoSeguroActivo) return;
  if (autoTransicionLuces) {
    if ((long)(millis() - parpadeoTransicionHastaMs) < 0) {
      if (millis() - ultimaParpadeoMs >= INTERVALO_PARPADEO_MS) {
        ultimaParpadeoMs = millis();
        for (int i = 0; i < TOTAL_SALIDAS; ++i) {
          if (i == 0 || i == 3) continue;
          if (!SALIDA_FISICA_CASA[i]) continue;
          ejecutarComandoRele("IR_AUTO_PARPADEO", i, !estadoSalidas[i], ORIGEN_AUTOMATICO);
        }
      }
      return;
    }
    autoTransicionLuces = false;
    alternarTodasLucesIR(false, "IR_AUTO_LUCES_OFF");
    emitirEventoLocal("ACK;IR;MODO_AUTO_LUCES_OFF");
    return;
  }
  automatizarInvernadero();
}

// AUTO = invernadero: bomba ON si el suelo está seco (crudo ≥ umbral),
// OFF al humedecer (histéresis). Luces no se tocan aquí.
void automatizarInvernadero() {
  static unsigned long ultimaEvalMs = 0;
  if (millis() - ultimaEvalMs < INTERVALO_RIEGO_MS) return;
  ultimaEvalMs = millis();
  if (ultimaHumedadValida < 0) return;
  const bool bombaOn = estadoSalidas[0];
  if (!bombaOn && ultimaHumedadValida >= UMBRAL_RIEGO_SEC_CRUDO) {
    ejecutarComandoRele("RIEGO_AUTO_ON", 0, true, ORIGEN_AUTOMATICO);
  } else if (bombaOn && ultimaHumedadValida <= UMBRAL_RIEGO_HUM_CRUDO) {
    ejecutarComandoRele("RIEGO_AUTO_OFF", 0, false, ORIGEN_AUTOMATICO);
  }
}

void ejecutarTeclaIRCasa(IRCasa::Tecla tecla) {
  using namespace IRCasa;
  switch (tecla) {
    // --- TOPE: modos y luces (demo del jurado) ---
    case CH_MENOS:
      fijarModoManualIR();
      break;
    case CH:
      alternarSalidaIR(4, "SPARE", "Spare");
      break;
    case CH_MAS:
      fijarModoAutoIR();
      break;
    case ANTERIOR: case N_1:
      alternarSalidaIR(1, "LUZ1", "Casa");
      break;
    case SIGUIENTE: case N_2:
      alternarSalidaIR(2, "LUZ2", "Porche");
      break;
    case PLAY: {
      // Antes silencio (sin audio): todas las luces ON/OFF de un toque.
      bool encender = !estadoSalidas[1] && !estadoSalidas[2] && !estadoSalidas[4];
      alternarTodasLucesIR(encender, "IR_TODAS");
      emitirEventoLocal(encender ? "ACK;IR;LUCES_TODAS_ON" : "ACK;IR;LUCES_TODAS_OFF");
      break;
    }
    case VOL_MENOS:
      alternarSalidaIR(0, "RIEGO", "Riego");
      break;
    case VOL_MAS:
      // Botón físico de demo movido al mando (antes volumen).
      if (estadoDemo == DEMO_DETENIDO) {
        modoLucesIR = MODOLUCES_MANUAL;
        iniciarSecuenciaDemo();
        emitirEventoLocal("ACK;IR;DEMO_ON");
      } else {
        detenerSecuenciaDemo();
        emitirEventoLocal("ACK;IR;DEMO_OFF");
      }
      break;
    case EQ:
      // Antes reporte (sin pantalla): apagado general de emergencia.
      for (int i = 0; i < TOTAL_SALIDAS; ++i)
        ejecutarComandoRele("IR_TODO_OFF", i, false, ORIGEN_IR);
      emitirEventoLocal("ACK;IR;TODO_OFF");
      break;

    // --- Fila media: seguridad y atajos ---
    case N_0:
      for (int i = 0; i < TOTAL_SALIDAS; ++i)
        ejecutarComandoRele("IR_TODO_OFF", i, false, ORIGEN_IR);
      emitirEventoLocal("ACK;IR;TODO_OFF;TECLA_0");
      break;
    case N_100_MAS:
      // Antes cambiar voz (sin audio): alterna MANUAL/AUTO.
      if (modoLucesIR == MODOLUCES_AUTO) fijarModoManualIR();
      else fijarModoAutoIR();
      break;
    case N_200_MAS:
      if (rearmarSistema()) emitirEventoLocal("ACK;IR;REARME_OK");
      else emitirEventoLocal("NACK;IR;REARME_BLOQ");
      break;

    // --- Abajo: alias numéricos usables (3 ya no es NACK) ---
    case N_3:
      alternarSalidaIR(4, "SPARE", "Spare");
      break;
    case N_4: {
      bool encender = !estadoSalidas[1] && !estadoSalidas[2] && !estadoSalidas[4];
      alternarTodasLucesIR(encender, "IR_TODAS");
      emitirEventoLocal(encender ? "ACK;IR;LUCES_TODAS_ON" : "ACK;IR;LUCES_TODAS_OFF");
      break;
    }
    case N_5:
      alternarSalidaIR(0, "RIEGO", "Riego");
      break;

    // --- Últimas: consultas solo por serie (sin LCD ni voz) ---
    case N_6:
    case N_7:
    case N_8:
    case N_9:
      emitirEventoLocal(construirReporteEstado());
      break;
    default: break;
  }
}

bool procesarComandoIR(const String &comando) {
  if (!comando.startsWith("IR")) return false;
  if (!IR_CASA_HABILITADO) {
    emitirEventoLocal("NACK;IR;DESHABILITADO_EN_PERFIL");
    return true;
  }
  if (comando == "IR_LEER") {
    emitirEventoLocal("IR;ULTIMO=0x" + String(receptorIR.ultimoCodigo(), HEX));
    return true;
  }
  if (comando == "IR_LISTA") {
    emitirEventoLocal("IR;APRENDIDAS=" + String(receptorIR.totalAprendidas()) +
      "/21;ESTADO=" + String(receptorIR.mapaCompleto() ? "COMPLETO" : "INCOMPLETO"));
    for (uint8_t i = 0; i < IRCasa::TOTAL; ++i)
      emitirEventoLocal("IR;INDICE=" + String(i) + ";TECLA=" +
        IRCasa::Receptor::nombre(i) + ";APRENDIDA=" +
        String(receptorIR.aprendida(i) ? 1 : 0) + ";CMD=0x" +
        String(receptorIR.codigo(i), HEX));
    return true;
  }
  if (comando == "IR_BORRAR") {
    receptorIR.borrar();
    emitirEventoLocal("ACK;IR_BORRAR;MAPA_SIN_APRENDER;SALIDAS_IR_BLOQUEADAS");
    return true;
  }
  if (comando.startsWith("IR_GRABAR_")) {
    String numero = comando.substring(10);
    for (unsigned int i = 0; i < numero.length(); ++i)
      if (!isDigit(numero[i])) { emitirEventoLocal("NACK;IR_GRABAR;INDICE_0_20"); return true; }
    int indice = numero.toInt();
    if (numero.length() == 0 || indice < 0 || indice >= IRCasa::TOTAL) {
      emitirEventoLocal("NACK;IR_GRABAR;INDICE_0_20");
      return true;
    }
    teclaIrPendiente = static_cast<int8_t>(indice);
    emitirEventoLocal("ACK;IR_GRABAR;PULSA=" + String(IRCasa::Receptor::nombre(indice)));
    return true;
  }
  emitirEventoLocal("NACK;IR;USA_IR_LEER_LISTA_GRABAR_N_O_BORRAR");
  return true;
}

// Puerta ordenada del mando: mientras Jarvis habla o no ha pasado
// BLOQUEO_IR_TRAS_ORDEN_MS desde la última tecla aceptada, toda señal IR
// se rechaza con NACK;IR;OCUPADO. La saltan el aprendizaje (IR_GRABAR_*),
// la tecla 0 (apagado general inmediato) y, fuera de aquí, el PARO físico
// y el comando Serial PARO, que nunca se bloquean.
#define BLOQUEO_IR_TRAS_ORDEN_MS 1500UL
unsigned long irBloqueadoHastaMs = 0;

bool irPuertaOcupada(IRCasa::Tecla tecla) {
  if (tecla == IRCasa::N_0) return false;  // apagado general: siempre pasa
  if (jarvisAudio.ocupado()) return true;  // DFPlayer hablando (pin BUSY)
  return (long)(irBloqueadoHastaMs - millis()) > 0;  // ventana tras la orden
}

void revisarIRCasa() {
  if (!IR_CASA_HABILITADO) return;
  IRCasa::Evento evento = receptorIR.actualizar();
  if (!evento.hay) return;
  pantallaFinal.mostrarIR(evento.protocolo, evento.direccion, evento.codigo);
  emitirEventoLocal("IR;PROTO=" + String(evento.protocolo) + ";ADDR=0x" +
    String(evento.direccion, HEX) + ";CMD=0x" + String(evento.codigo, HEX) + ";TECLA=" +
    String(IRCasa::Receptor::nombre(evento.tecla)) + ";REP=" + String(evento.repeticion ? 1 : 0));
  if (teclaIrPendiente >= 0) {
    const int indice = teclaIrPendiente;
    teclaIrPendiente = -1;
    bool guardada = receptorIR.grabar(indice, evento.codigo);
    emitirEventoLocal(String(guardada ? "ACK" : "NACK") + ";IR_GRABAR;" +
      IRCasa::Receptor::nombre(indice) + ";CMD=0x" + String(evento.codigo, HEX) +
      ";" + receptorIR.ultimoError());
    return;
  }
  if (evento.tecla == IRCasa::NINGUNA) {
    emitirEventoLocal("NACK;IR;TECLA_DESCONOCIDA");
    return;
  }
  if (irPuertaOcupada(evento.tecla)) {
    emitirEventoLocal("NACK;IR;OCUPADO");
    return;
  }
  ejecutarTeclaIRCasa(evento.tecla);
  irBloqueadoHastaMs = millis() + BLOQUEO_IR_TRAS_ORDEN_MS;
}

void restaurarModoAutomatico(const String &comando, int indice) {
  if (indice < 0 || indice >= TOTAL_SALIDAS) {
    emitirEventoLocal("NACK;" + comando + ";indice_invalido");
    return;
  }
  propietarioSalidas[indice] = PROPIETARIO_AUTOMATICO;
  log("MODO", String(NOMBRES_SALIDAS[indice]) + " -> AUTO");
  emitirEventoLocal("ACK;" + comando + ";AUTO");
}

void activarParoEmergencia(const char* motivo) {
  paroEmergenciaActivo = true;
  for (int i = 0; i < TOTAL_SALIDAS; i++) {
    OrdenActuador orden = {i, false, ORIGEN_SISTEMA, 1.0f, motivo};
    ejecutarOrdenActuador(orden);
  }
  emitirEventoLocal("EVENTO;PARO_EMERGENCIA;ACTIVO");
  anunciarJarvisGrupo(EventoJarvis::TECLA_0, false, 3, 4);
}

void entrarModoSeguro(const char* motivo) {
  if (modoSeguroActivo) return;
  modoSeguroActivo = true;
  strncpy(motivoModoSeguro, motivo ? motivo : "desconocido", sizeof(motivoModoSeguro) - 1);
  motivoModoSeguro[sizeof(motivoModoSeguro) - 1] = '\0';
  for (int i = 0; i < TOTAL_SALIDAS; i++) {
    OrdenActuador orden = {i, false, ORIGEN_SISTEMA, 1.0f, "MODO_SEGURO"};
    ejecutarOrdenActuador(orden);
  }
  emitirEventoLocal("EVENTO;MODO_SEGURO;" + String(motivoModoSeguro));
}

bool recuperarModoSeguro() {
  if (!watchdogActivo) {
    emitirEventoLocal("NACK;RECUPERAR;watchdog_no_disponible");
    return false;
  }
  if (!modoSeguroActivo) {
    emitirEventoLocal("ACK;RECUPERAR;YA_ESTABLE");
    return true;
  }
  if (paroEmergenciaActivo) {
    emitirEventoLocal("NACK;RECUPERAR;paro_emergencia");
    return false;
  }
  const unsigned long memoriaLibre = esp_get_free_heap_size();
  if (memoriaLibre < MEMORIA_LIBRE_RECUPERACION_BYTES) {
    emitirEventoLocal("NACK;RECUPERAR;memoria_insuficiente");
    return false;
  }

  modoSeguroActivo = false;
  strncpy(motivoModoSeguro, "ninguno", sizeof(motivoModoSeguro));
  motivoModoSeguro[sizeof(motivoModoSeguro) - 1] = '\0';
  // Las cargas permanecen MANUAL_OFF. El operador debe devolver cada una a
  // AUTO o encenderla explícitamente después de revisar el diagnóstico.
  emitirEventoLocal("ACK;RECUPERAR;SEGURO");
  return true;
}

bool rearmarSistema() {
  if (digitalRead(MAPA_CASA.paro) == LOW) {
    emitirEventoLocal("NACK;REARMAR;boton_emergencia_presionado");
    return false;
  }
  paroEmergenciaActivo = false;
  // El rearme no enciende nada ni devuelve cargas a AUTO por sí solo.
  emitirEventoLocal("ACK;REARMAR;SEGURO");
  return true;
}

void cargarCalibracion() {
  Preferences prefs;
  if (prefs.begin("domus-cal", true)) {
    CalibracionDomus leida = {};
    if (prefs.getBytesLength("config") == sizeof(leida) &&
        prefs.getBytes("config", &leida, sizeof(leida)) == sizeof(leida) &&
        calibracionValida(leida) && leida.checksum == checksumCalibracion(leida)) {
      calibracion = leida;
      calibracionGuardada = true;
    }
    prefs.end();
  }
  calibracionPendiente = calibracion;
}

bool procesarCalibracion(const String &comando) {
  if (!comando.startsWith("CAL_")) return false;
  if (comando == "CAL_VER") {
    emitirEventoLocal("CAL;SECO=" + String(calibracionPendiente.sueloSeco) +
      ";HUMEDO=" + String(calibracionPendiente.sueloHumedo) +
      ";OSCURO=" + String(calibracionPendiente.luzOscura) +
      ";CLARO=" + String(calibracionPendiente.luzClara) +
      ";NIVEL=" + String(calibracionPendiente.nivelMinimo));
    return true;
  }
  // NVS solo se modifica con paro enclavado y todas las salidas apagadas.
  bool apagadas = true;
  for (bool estado : estadoSalidas) apagadas = apagadas && !estado;
  if (!paroEmergenciaActivo || !apagadas) {
    emitirEventoLocal("NACK;CAL;requiere_paro");
    return true;
  }
  if (comando == "CAL_GUARDAR") {
    if (!calibracionValida(calibracionPendiente)) {
      emitirEventoLocal("NACK;CAL;rangos_invalidos");
      return true;
    }
    calibracionPendiente.checksum = checksumCalibracion(calibracionPendiente);
    // Una misma configuración no vuelve a desgastar la flash.
    bool guardada = calibracionGuardada &&
      calibracion.checksum == calibracionPendiente.checksum;
    if (!guardada) {
      Preferences prefs;
      if (prefs.begin("domus-cal", false)) {
        guardada = prefs.putBytes("config", &calibracionPendiente,
                                 sizeof(calibracionPendiente)) == sizeof(calibracionPendiente);
        prefs.end();
      }
    }
    if (guardada) {
      calibracion = calibracionPendiente;
      calibracionGuardada = true;
    }
    emitirEventoLocal(guardada ? "ACK;CAL_GUARDAR;OK" : "NACK;CAL;error_nvs");
    return true;
  }
  if (comando == "CAL_CANCELAR") {
    calibracionPendiente = calibracion;
    emitirEventoLocal("ACK;CAL_CANCELAR;OK");
    return true;
  }
  int separador = comando.indexOf('=');
  if (separador < 0) { emitirEventoLocal("NACK;CAL;sintaxis"); return true; }
  String numero = comando.substring(separador + 1);
  if (numero.length() == 0 || numero.length() > 4) {
    emitirEventoLocal("NACK;CAL;numero_invalido"); return true;
  }
  for (unsigned int i = 0; i < numero.length(); i++) {
    if (numero[i] < '0' || numero[i] > '9') {
      emitirEventoLocal("NACK;CAL;numero_invalido"); return true;
    }
  }
  int valor = numero.toInt();
  if (valor > 4095) { emitirEventoLocal("NACK;CAL;fuera_de_adc"); return true; }
  String clave = comando.substring(0, separador);
  if (clave == "CAL_SECO") calibracionPendiente.sueloSeco = valor;
  else if (clave == "CAL_HUMEDO") calibracionPendiente.sueloHumedo = valor;
  else if (clave == "CAL_OSCURO") calibracionPendiente.luzOscura = valor;
  else if (clave == "CAL_CLARO") calibracionPendiente.luzClara = valor;
  else { emitirEventoLocal("NACK;CAL;clave_invalida"); return true; }
  emitirEventoLocal("ACK;CAL;PENDIENTE_GUARDAR");
  return true;
}

// Tabla comando -> salida: una sola comparación por entrada en vez de
// decenas de ramas if/else con String temporales. SPARE = índice 4 (GPIO6,
// 2 LEDs azules de Jarvis). Cultivo no tiene luz: sin comandos.
struct EntradaComandoRele {
  const char* nombre;
  int8_t indice;
  int8_t encender;  // 1 = ON, 0 = OFF, -1 = devolver a AUTO
};
static const EntradaComandoRele TABLA_COMANDOS_RELE[] = {
  {"RIEGO_ON", 0, 1}, {"RIEGO_OFF", 0, 0}, {"RIEGO_AUTO", 0, -1},
  {"LUZ1_ON", 1, 1}, {"LUZ1_OFF", 1, 0}, {"LUZ1_AUTO", 1, -1},
  {"LUZ2_ON", 2, 1}, {"LUZ2_OFF", 2, 0}, {"LUZ2_AUTO", 2, -1},
  {"SPARE_ON", 4, 1}, {"SPARE_OFF", 4, 0}, {"SPARE_AUTO", 4, -1},
};

bool despacharComandoRele(const String &comando) {
  for (size_t i = 0; i < sizeof(TABLA_COMANDOS_RELE) / sizeof(TABLA_COMANDOS_RELE[0]); ++i) {
    const EntradaComandoRele &e = TABLA_COMANDOS_RELE[i];
    if (comando == e.nombre) {
      if (e.encender < 0) restaurarModoAutomatico(comando, e.indice);
      else ejecutarComandoRele(comando, e.indice, e.encender == 1);
      return true;
    }
  }
  return false;
}

// Diagnóstico de flotancia: PINTEST <gpio> reporta crudo, con pull-up y
// con pull-down. Un pin flotante sigue los pulls (alto/bajo); un sensor
// real apenas se mueve. No entra a COMANDOS_VALIDOS: es solo diagnóstico.
bool procesarPinTest(const String &comando) {
  if (!comando.startsWith("PINTEST")) return false;
  String numero = comando.substring(7);
  numero.trim();
  for (unsigned int i = 0; i < numero.length(); ++i) {
    if (!isDigit(numero[i])) {
      emitirEventoLocal("NACK;PINTEST;USA_PINTEST_GPIO");
      return true;
    }
  }
  const int pin = numero.toInt();
  if (numero.length() == 0 || pin < 0 || pin > 48) {
    emitirEventoLocal("NACK;PINTEST;GPIO_0_48");
    return true;
  }
  const int crudo = analogRead(pin);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(500);
  const int conPullUp = analogRead(pin);
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(500);
  const int conPullDown = analogRead(pin);
  bool esSalida = false;
  for (int i = 0; i < TOTAL_SALIDAS; ++i) {
    if (MAPA_CASA.salidas[i] == pin) { esSalida = true; break; }
  }
  if (esSalida) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, nivelSalida(pin == MAPA_CASA.salidas[0] ? 0 :
      pin == MAPA_CASA.salidas[1] ? 1 :
      pin == MAPA_CASA.salidas[2] ? 2 :
      pin == MAPA_CASA.salidas[3] ? 3 : 4, false));
  } else {
    pinMode(pin, INPUT);
  }
  char linea[96];
  snprintf(linea, sizeof(linea), "PINTEST;GPIO=%d;CRUDO=%d;PULLUP=%d;PULLDOWN=%d;%s",
           pin, crudo, conPullUp, conPullDown,
           (conPullUp > 3500 && conPullDown < 600) ? "FLOTANTE" : "CONECTADO");
  emitirEventoLocal(linea);
  return true;
}

void procesarComandoTexto(const String &comandoCrudo) {
  // Copia local editable: el parámetro llega por referencia constante para
  // no duplicar el buffer en cada comando (antes se pasaba por valor).
  String comando = comandoCrudo;
  comando.trim();
  comando.toUpperCase();

  if (comando.length() == 0) return;

  if (comando.length() > 20) {
    char detalle[64];
    snprintf(detalle, sizeof(detalle), "Longitud invalida (%u caracteres)",
             (unsigned)comando.length());
    registrarError("COMANDO", detalle);
    snprintf(detalle, sizeof(detalle), "NACK;%.20s;longitud_invalida",
             comando.c_str());
    emitirEventoLocal(detalle);
    return;
  }

  // PARO siempre atraviesa el limitador. El resto se limita para que una
  // terminal defectuosa no monopolice CPU, memoria ni escritura en microSD.
  if (comando != "PARO" && !permitirComandoSerial()) {
    emitirEventoLocal("NACK;" + comando + ";limite_de_frecuencia");
    return;
  }

   if (procesarCalibracion(comando)) return;
   if (procesarComandoIR(comando)) return;
   if (procesarPinTest(comando)) return;
   if (comando == "PINTEST_ALL") {
     emitirEventoLocal("PINTEST;INICIO;GPIO_3_18");
     for (int pin = 3; pin <= 18; ++pin) {
       const int crudo = analogRead(pin);
       pinMode(pin, INPUT_PULLUP);
       delayMicroseconds(500);
       const int conPullUp = analogRead(pin);
       pinMode(pin, INPUT_PULLDOWN);
       delayMicroseconds(500);
       const int conPullDown = analogRead(pin);
       pinMode(pin, INPUT);
       char linea[96];
       snprintf(linea, sizeof(linea), "PINTEST;GPIO=%d;CRUDO=%d;PULLUP=%d;PULLDOWN=%d;%s",
                pin, crudo, conPullUp, conPullDown,
                (conPullUp > 3500 && conPullDown < 600) ? "FLOTANTE" : "CONECTADO");
       emitirEventoLocal(linea);
     }
     emitirEventoLocal("PINTEST;FIN");
     return;
   }
   if (!esComandoValido(comando)) {
    registrarError("COMANDO", "No reconocido: " + comando);
    emitirEventoLocal("NACK;" + comando + ";no_reconocido");
    return;
  }

  log("SERIAL", "Comando recibido: " + comando);

  if (despacharComandoRele(comando)) return;
  else if (comando == "TODO_ON") {
    // Enciende luces + Spare (índices 1,2,4); no toca la bomba (0) ni
    // Cultivo (3, sin etapa).
    for (int i = 1; i < TOTAL_SALIDAS; ++i)
      if (SALIDA_FISICA_CASA[i])
        ejecutarComandoRele("TODO_ON", i, true, ORIGEN_MANUAL);
  }
  else if (comando == "TODO_OFF") {
    for (int i = 0; i < TOTAL_SALIDAS; ++i)
      ejecutarComandoRele("TODO_OFF", i, false, ORIGEN_MANUAL);
  }
  else if (comando == "DEMO_ON") {
    iniciarSecuenciaDemo();
    emitirEventoLocal("ACK;DEMO_ON");
  }
  else if (comando == "DEMO_OFF") {
    detenerSecuenciaDemo();
    emitirEventoLocal("ACK;DEMO_OFF");
  }
  else if (comando == "ESTADO")    emitirEventoLocal(construirReporteEstado());
  else if (comando == "DIAGNOSTICO") emitirEventoLocal(construirReporteDiagnostico());
  else if (comando == "PRUEBA") emitirPruebaGuiada();
  else if (comando == "PARO") activarParoEmergencia("PARO_SERIAL");
  else if (comando == "REARMAR") rearmarSistema();
  else if (comando == "RECUPERAR") recuperarModoSeguro();
  else if (comando == "MIC_ESTADO") emitirEventoLocal(micHabilitado ? "MIC;ON" : "MIC;OFF");
  else if (comando == "REPETIR") emitirEventoLocal(jarvisAudio.repetirUltima(millis())
    ? "ACK;REPETIR" : "NACK;REPETIR;AUDIO_NO_DISPONIBLE");
  else if (comando == "SD_PRUEBA") emitirEventoLocal(solicitarPruebaSD() ? "ACK;SD_PRUEBA;ENCOLADA" : "NACK;SD_PRUEBA;NO_DISPONIBLE");
}

// La tarea microSD es la única que toca la tarjeta: aquí solo se encola.
// Sin SD montada es no-op y no se reserva ni un byte de heap.
void registrarLineaMicroSD(const char* linea) {
  if (!MICROSD_HABILITADA || !microSdMontada || colaSD == nullptr || linea == nullptr) return;
  TrabajoSD trabajo = {};
  trabajo.prueba = false;
  trabajo.momento = millis();
  strncpy(trabajo.linea, linea, sizeof(trabajo.linea) - 1);
  trabajo.linea[sizeof(trabajo.linea) - 1] = '\0';
  if (xQueueSend(colaSD, &trabajo, 0) != pdTRUE) sdDescartados++;
}
inline void registrarLineaMicroSD(const String &linea) {
  registrarLineaMicroSD(linea.c_str());
}

void emitirEventoLocal(const char* linea) {
  Serial.println(linea);
  registrarLineaMicroSD(linea);
}
inline void emitirEventoLocal(const String &linea) {
  emitirEventoLocal(linea.c_str());
}

void revisarComandosSerial() {
  // Ceder al lazo principal aun cuando el host transmita continuamente.
  unsigned int bytesProcesados = 0;
  while (Serial.available() > 0 && bytesProcesados < 128) {
    bytesProcesados++;
    char caracter = (char)Serial.read();
    if (descartarComandoHastaNuevaLinea) {
      if (caracter == '\n') descartarComandoHastaNuevaLinea = false;
      continue;
    }
    if (caracter == '\r') continue;
    if (caracter == '\n') {
      if (bufferComandoSerial.length() > 0) {
        procesarComandoTexto(bufferComandoSerial);
        bufferComandoSerial = "";
      }
      continue;
    }
    if (bufferComandoSerial.length() < 40) {
      bufferComandoSerial += caracter;
    } else {
      bufferComandoSerial = "";
      descartarComandoHastaNuevaLinea = true;
      emitirEventoLocal("NACK;SERIAL;buffer_excedido");
    }
  }
}

void revisarControlesFisicos() {
  // Botones SILENCIO y MODO leidos directamente desde GPIO
  bool nuevoMicHabilitado = digitalRead(MAPA_CASA.micOff) != LOW;
  if (nuevoMicHabilitado != micHabilitado) {
    micHabilitado = nuevoMicHabilitado;
    jarvisAudio.silenciar(!micHabilitado);
    emitirEventoLocal(String("EVENTO;SILENCIO;") + (micHabilitado ? "OFF" : "ON"));
  }
  if (digitalRead(MAPA_CASA.paro) == LOW) {
    if (!paroEmergenciaActivo) activarParoEmergencia("PARO_FISICO");
  }
  // MODO/LCD descartado: sin pantalla no hay cambio de vista.
}

// ============================================================================
// ============================================================================
// SECCIÓN 12B: SUPERVISOR DE SALUD
// ============================================================================
bool esReinicioCritico(esp_reset_reason_t motivo) {
  return motivo == ESP_RST_PANIC || motivo == ESP_RST_INT_WDT ||
         motivo == ESP_RST_TASK_WDT || motivo == ESP_RST_WDT ||
         motivo == ESP_RST_BROWNOUT;
}

void supervisarSalud() {
  const unsigned long ahora = millis();
  if (ahora - ultimaRevisionSaludMs < INTERVALO_SUPERVISOR_MS) return;
  ultimaRevisionSaludMs = ahora;

  const unsigned long memoriaLibre = esp_get_free_heap_size();
  if (memoriaLibre < memoriaLibreMinima) memoriaLibreMinima = memoriaLibre;

  if (memoriaLibre < MEMORIA_LIBRE_CRITICA_BYTES) {
    entrarModoSeguro("memoria_critica");
  }

  // Tras un minuto estable se rompe la cadena de reinicios críticos. El dato
  // vive en RTC RAM y no desgasta la flash/NVS.
  if (!contadorReiniciosEstabilizado && ahora >= TIEMPO_ARRANQUE_ESTABLE_MS) {
    reiniciosCriticosConsecutivos = 0;
    contadorReiniciosEstabilizado = true;
    log("SALUD", "Arranque estable confirmado; contador de reinicios limpiado");
  }
}

// ============================================================================
// SECCIÓN 13: SETUP
// ============================================================================
bool inicializarWatchdog() {
  esp_task_wdt_config_t configWdt = {
    .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_err_t resultado = esp_task_wdt_init(&configWdt);
  if (resultado == ESP_ERR_INVALID_STATE) {
    resultado = esp_task_wdt_reconfigure(&configWdt);
  }
  if (resultado != ESP_OK) return false;
  if (esp_task_wdt_status(NULL) != ESP_OK && esp_task_wdt_add(NULL) != ESP_OK) return false;
  return esp_task_wdt_reset() == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  log("SISTEMA", "=== PROJECT DOMUS v6 offline - Iniciando ===");

  const esp_reset_reason_t motivoReinicio = esp_reset_reason();
  if (esReinicioCritico(motivoReinicio)) {
    if (reiniciosCriticosConsecutivos < 255) reiniciosCriticosConsecutivos++;
  } else {
    reiniciosCriticosConsecutivos = 0;
  }
  const bool arrancarEnModoSeguro = reiniciosCriticosConsecutivos >= 3;

  // Reserva una sola vez el búfer de entrada para evitar realocaciones y
  // fragmentación del heap durante una sesión larga de diagnóstico.
  bufferComandoSerial.reserve(41);

  pinMode(MAPA_CASA.paro, INPUT_PULLUP);
  pinMode(MAPA_CASA.micOff, INPUT_PULLUP);
  pinMode(MAPA_CASA.demo, INPUT_PULLUP);
  micHabilitado = digitalRead(MAPA_CASA.micOff) != LOW;

  for (int i = 0; i < TOTAL_SALIDAS; i++) {
    // En perfil 4, AIN1/AIN2 del DRV8833 los configura el init del driver:
    // no precargarlos aquí (evita AIN1 HIGH con AIN2 flotando en el arranque).
    // Con bomba por GPIO directo no se reservan AIN1/AIN2: GPIO4 sale como
    // OUTPUT normal en el arranque (el DRV queda fuera del camino).
    const bool pinEsPuentH = PERFIL_CON_DRV8833 && !BOMBA_DIRECTA_S8050 &&
        (MAPA_CASA.salidas[i] == DRV8833_PIN_AIN1 ||
         MAPA_CASA.salidas[i] == DRV8833_PIN_AIN2);
    if (!pinEsPuentH) {
      // Precarga el nivel inactivo antes de habilitar la salida para reducir
      // pulsos breves durante el arranque en módulos activos en LOW.
      // Arranque OFF: las salidas sin etapa quedan en INPUT (nota 53/54).
      digitalWrite(MAPA_CASA.salidas[i], nivelSalida(i, false));
      pinMode(MAPA_CASA.salidas[i], SALIDA_FISICA_CASA[i] ? OUTPUT : INPUT);
    }
    propietarioSalidas[i] = PROPIETARIO_NINGUNO;
  }
  // El perfil con S8050 arranca con la bomba en manual OFF. Así una lectura
  // provisional de suelo no inicia riego al conectar la alimentación. Para
  // probar automatización el operador debe enviar RIEGO_AUTO explícitamente.
  if (BOMBA_DIRECTA_S8050) propietarioSalidas[0] = PROPIETARIO_MANUAL_OFF;
  log("SISTEMA", "Salidas inicializadas (todas apagadas)");
  watchdogActivo = inicializarWatchdog();
  if (!watchdogActivo) entrarModoSeguro("watchdog_no_disponible");
  else log("SISTEMA", "Watchdog verificado: " + String(WATCHDOG_TIMEOUT_S) + "s");
  cargarCalibracion();

  if (arrancarEnModoSeguro) {
    entrarModoSeguro("reinicios_criticos_consecutivos");
  }

  detectarPantalla();
  pantallaFinal.begin(lcd);
  if (watchdogActivo) esp_task_wdt_reset();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  inicializarMicroSD();
  if (watchdogActivo) esp_task_wdt_reset();

  if (IR_CASA_HABILITADO) {
    receptorIR.begin(MAPA_CASA.ir);
    log("IR", "HX1838 iniciado en GPIO12; usa IR_GRABAR_0 hasta IR_GRABAR_20");
  }
if (BOMBA_DIRECTA_S8050)
     log("BANCO", "Bomba GPIO directa bloqueada al arrancar; usa RIEGO_ON o modo AUTO");

  // Perfil final: inicializar DRV8833
  if (PERFIL_CON_DRV8833) {
    // DRV8833: AIN1=GPIO4, AIN2=GPIO7 (bomba canal A)
    // Canal B (ventilador) eliminado
    pinMode(DRV8833_PIN_AIN1, OUTPUT);
    pinMode(DRV8833_PIN_AIN2, OUTPUT);
    digitalWrite(DRV8833_PIN_AIN1, LOW);
    digitalWrite(DRV8833_PIN_AIN2, LOW);
    log("DRV8833", "DRV8833 inicializado (bomba canal A, canal B eliminado)");
  }

  // DFPlayer fuera: transporteDFPlayer.begin(MAPA_CASA.sda, MAPA_CASA.demo, MP3_BUSY_PIN) no se llama.
  // DFPlayer/DHT/LCD fuera de la demo feria: solo IR + luces.
  log("JARVIS", "Control por IR activo; sin audio, sin DHT, sin LCD");

  delay(1000);
  log("SISTEMA", "=== Sistema listo ===");
}

// ============================================================================
// SECCIÓN 13B: SECUENCIAS DE DEMOSTRACIÓN Y AUTOMATIZACIONES RELATIVAS
// ============================================================================

// Secuencia de demostración: alterna todas las luces en un patrón.
// No bloquea el sistema principal. El enum/estado viven junto al mapa IR.
unsigned long ultimaDemoCambioMs = 0;
const unsigned long INTERVALO_DEMO_MS = 2000; // 2 segundos entre cambios

void iniciarSecuenciaDemo() {
  if (estadoDemo == DEMO_DETENIDO) {
    estadoDemo = DEMO_EN_CURSO;
    ultimaDemoCambioMs = millis();
    log("DEMO", "Secuencia de demostración iniciada");
  }
}

void detenerSecuenciaDemo() {
  estadoDemo = DEMO_DETENIDO;
  log("DEMO", "Secuencia de demostración detenida");
}

void demoSecuenciaActualizar() {
  if (estadoDemo != DEMO_EN_CURSO) return;
  if (millis() - ultimaDemoCambioMs < INTERVALO_DEMO_MS) return;
  ultimaDemoCambioMs = millis();

  // Alterna todas las salidas instaladas pasando por el despachador: así
  // la demo respeta interlocks (nivel, PARO, driver), propiedad, historial
  // y voz. Antes escribía el GPIO directo y podía regar en seco.
  for (int i = 0; i < TOTAL_SALIDAS; i++) {
    if (SALIDA_FISICA_CASA[i]) {
      ejecutarComandoRele("DEMO_TOGGLE", i, !estadoSalidas[i], ORIGEN_MANUAL);
    }
  }
}

// ============================================================================
// SECCIÓN 13C: TELEMETRÍA DE SENSORES (stream SENSORES; para HIL)
// ============================================================================
// El HIL (test_07) espera una línea SENSORES; cada pocos segundos. Solo
// usa últimos valores válidos: no muestrea hardware, no bloquea, no toca
// la SD (es stream, no bitácora) y funciona también en modo seguro.
#define INTERVALO_TELEMETRIA_SENSORES_MS 2000UL
unsigned long ultimaTelemetriaSensoresMs = 0;

void emitirTelemetriaSensores() {
  if (millis() - ultimaTelemetriaSensoresMs < INTERVALO_TELEMETRIA_SENSORES_MS) return;
  ultimaTelemetriaSensoresMs = millis();
  char linea[160];
  snprintf(linea, sizeof(linea),
    "SENSORES;TEMP_C=%.1f;HUM_AIRE=%.1f;HUM_PCT=%d;LUZ_PCT=%d;"
    "PRESENCIA=1;SALIDAS=%d%d%d%d%d;",
    (double)ultimaTempCValida, (double)ultimaHumAireValida,
    ultimoHumedadPctValido, ultimoLuzPctValido,
    estadoSalidas[0] ? 1 : 0, estadoSalidas[1] ? 1 : 0,
    estadoSalidas[2] ? 1 : 0, estadoSalidas[3] ? 1 : 0,
    estadoSalidas[4] ? 1 : 0);
  Serial.println(linea);
}

// ============================================================================
// SECCIÓN 13D: DIAGNÓSTICO ROBUSTO (tecla EQ)
// ============================================================================
// Lee todos los sensores, muestra cada uno en LCD (2s), reproduce audio
// Jarvis según resultado, y reporta por Serial.

// ============================================================================
// SECCIÓN 14: LOOP PRINCIPAL (no bloqueante)
// ============================================================================

void loop() {
  if (watchdogActivo && esp_task_wdt_reset() != ESP_OK) {
    watchdogActivo = false;
    entrarModoSeguro("watchdog_reset_fallo");
  }

  // 1. Seguridad y controles físicos tienen prioridad máxima.
  revisarControlesFisicos();
  supervisarSalud();
  revisarIRCasa();

  // 2. Diagnóstico/control local por USB, sin red ni aplicación móvil.
  revisarComandosSerial();

  // 3. Corte de bomba independiente del supervisor.
  verificarLimiteBomba();

  // 4. Secuencia de demo, transición AUTO y riego de invernadero.
  demoSecuenciaActualizar();
  parpadeoAutoActualizar();

  // 5. Muestrear suelo y LDR para ESTADO/SENSORES (sin DHT/LCD/DFPlayer).
  if (!modoSeguroActivo) {
    int crudo = 0, pct = 0;
    leerHumedad(crudo, pct);
    leerLuz(crudo, pct);
  }

  // 6. Telemetría en vivo para el HIL y el monitor.
  emitirTelemetriaSensores();
}
