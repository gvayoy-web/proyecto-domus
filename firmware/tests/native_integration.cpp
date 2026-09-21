// Hardware fakes only. Production functions are inserted by the Python runner.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "domus_types.h"
#include "domus_calibration.h"
// Stubs de voz/historial: el despachador real anuncia cada cambio por Jarvis
// y lo registra en microSD; en host esas salidas se neutralizan pero los
// símbolos deben existir para enlazar las funciones extraídas del .ino.
enum class EventoJarvis : uint8_t {
  CH_MENOS = 1, CH, CH_MAS, ANTERIOR, PLAY, SIGUIENTE, VOL_MENOS, VOL_MAS,
  EQ, TECLA_0, TECLA_100, TECLA_200, TECLA_1, TECLA_2, TECLA_3, TECLA_4,
  TECLA_5, TECLA_6, TECLA_7, TECLA_8, TECLA_9
};
inline bool anunciarJarvis(EventoJarvis, bool = false, uint8_t = 0) { return false; }
inline bool anunciarJarvisGrupo(EventoJarvis, bool, uint8_t, uint8_t) { return false; }
inline EventoJarvis carpetaSalida(int indice) {
  switch (indice) {
    case 0: return EventoJarvis::TECLA_5;
    case 1: return EventoJarvis::TECLA_1;
    case 2: return EventoJarvis::TECLA_2;
    case 3: return EventoJarvis::TECLA_4;
    case 4: return EventoJarvis::TECLA_3;
    default: return EventoJarvis::TECLA_9;
  }
}
enum class TipoRegistroHistorial : uint8_t {
  RIEGO_AUTO, RIEGO_MANUAL, LUZ_ENCENDIDA, LUZ_APAGADA,
  VENT_ENCENDIDO, VENT_APAGADO, PARO_EMERGENCIA, MODO_SEGURO,
  SENSOR_TEMP, SENSOR_HUMEDAD, SENSOR_LDR, SENSOR_PIR,
  SENSOR_NIVEL, CONFIGURACION, DIAGNOSTICO, DEMO_SECUENCIA
};
struct EstadisticasDOMUS {
  uint32_t totalEncendidos = 0, totalApagados = 0;
  uint32_t totalRiiegosAutomaticos = 0, totalVentAutomaticos = 0;
  uint32_t totalCambiosLuz = 0, totalEmergencias = 0, totalErroresSensores = 0;
  unsigned long tiempoEncendidoBomba = 0, tiempoEncendidoVentilador = 0;
};
EstadisticasDOMUS estadisticas;
inline void incrementarTotalEncendidos() { estadisticas.totalEncendidos++; }
inline void incrementarTotalApagados() { estadisticas.totalApagados++; }
inline void incrementarTotalRiiegosAutomaticos() { estadisticas.totalRiiegosAutomaticos++; }
inline void incrementarTotalVentAutomaticos() { estadisticas.totalVentAutomaticos++; }
inline void incrementarTotalCambiosLuz() { estadisticas.totalCambiosLuz++; }
inline void incrementarTotalEmergencias() { estadisticas.totalEmergencias++; }
inline void incrementarTotalErroresSensores() { estadisticas.totalErroresSensores++; }
inline void registrarHistorial(TipoRegistroHistorial, uint8_t, bool, float = 0) {}
struct String : std::string {
  using std::string::string;
  String(const std::string &s):std::string(s) {}
  String(int n):std::string(std::to_string(n)) {}
  String(float n,int):std::string(std::to_string(n)) {}
};
constexpr int LOW=0,HIGH=1,TOTAL_SALIDAS=5,PIN_PARO_EMERGENCIA=10;
constexpr int PIN_MIC_OFF=11;
bool micHabilitado=true;
constexpr int MAX_FALLOS_ANTES_DE_ALERTA_PERSISTENTE=3;
constexpr bool MP3_HABILITADO=false;
constexpr unsigned long TIEMPO_MAXIMO_BOMBA_MS=120000, MEMORIA_LIBRE_RECUPERACION_BYTES=65536;
enum PropietarioActuador {PROPIETARIO_NINGUNO, PROPIETARIO_MANUAL_ON,
                         PROPIETARIO_MANUAL_OFF, PROPIETARIO_AUTOMATICO};
int PINES_SALIDAS[5]={4,5,8,7,8}, gpio[64]={};
bool estadoSalidas[5]={}, SALIDA_ACTIVA_EN_BAJO[5]={true,true,true,true,true};
int fallosVerificacionSalida[5]={};
const char *NOMBRES_SALIDAS[5]={"Bomba","Casa","Porche","Cultivo","Spare"};
PropietarioActuador propietarioSalidas[5]={};
// Fake del mapa central (nota 55): mismos valores que MAPA_CASA en producción.
struct MapaPinesCasa {
  int suelo, nivel, ldr, bomba, casa, porche, cultivo, spare;
  int paro, micOff, demo, scl, dht, sda, ir;
  int salidas[5];
};
constexpr MapaPinesCasa MAPA_CASA = {
  15, 16, 3,
  4, 5, 8, 7, 8,
  10, 11, 18, 13, 14, 17, 12,
  {4, 5, 8, 7, 8}};
// Fakes de perfil (nota 54): máscara con TODAS las etapas + driver validado,
// para probar la lógica de despacho/seguridad/timeout con actuadores presentes.
// El rechazo sin etapa y sin driver lo cubren test_casa_candidato.py, el
// contrato de compilación y la matriz (all-false/all-disabled reales).
constexpr bool SALIDA_FISICA_CASA[5]={true,true,true,true,true};
constexpr bool BOMBA_DIRECTA_S8050=false;
inline bool driverMotoresListo() { return true; }
inline bool driverMotoresAplicarFinal(uint8_t canal, bool activar) { (void)canal; (void)activar; return true; }
bool paroEmergenciaActivo=false, modoSeguroActivo=false, watchdogActivo=true;
char motivoModoSeguro[48]="ninguno";
unsigned long bombaEncendidaDesdeMs=0, reloj=1000, heap=100000;
int agua=1000; bool sensorValido=true;
CalibracionDomus calibracion={1,2800,1200,3200,400,600,0};
std::vector<String> eventos;
void emitirEventoLocal(const String &s) {eventos.push_back(s);}
void registrarError(const String&,const String&) {}
void log(const String&,const String&) {}
void reproducirPista(int) {}
void responderJarvis(const String&) {}
String construirRespuestaJarvis(const OrdenActuador&,bool,const ResultadoOrden&) {return "respuesta";}
bool leerNivelAgua(int &salida) {salida=agua;return sensorValido;}
void digitalWrite(int pin,int valor) {gpio[pin]=valor;}
int digitalRead(int pin) {return gpio[pin];}
unsigned long millis() {return reloj;}
void delay(int n) {reloj+=n;}
unsigned long esp_get_free_heap_size() {return heap;}
// ACTUAL_FUNCTIONS
int main() {
  gpio[PIN_PARO_EMERGENCIA]=HIGH;
  gpio[PIN_MIC_OFF]=HIGH;
  for(int profile=0;profile<2;profile++) {
    for(int i=0;i<5;i++) SALIDA_ACTIVA_EN_BAJO[i]=(i==0 || profile==0);
    for(int i=0;i<5;i++) {
      assert(ejecutarOrdenActuador({i,true,ORIGEN_MANUAL,1,"on"}).exito);
      assert(estadoSalidas[i] && digitalRead(PINES_SALIDAS[i])==nivelSalida(i,true));
    }
    activarParoEmergencia("test");
    for(int i=0;i<5;i++) {
      assert(!estadoSalidas[i]);
      assert(!ejecutarOrdenActuador({i,true,ORIGEN_MANUAL,1,"on"}).exito);
    }
    gpio[PIN_PARO_EMERGENCIA]=LOW; assert(!rearmarSistema());
    gpio[PIN_PARO_EMERGENCIA]=HIGH; assert(rearmarSistema());
    assert(!estadoSalidas[0]);
    agua=0; assert(!ejecutarOrdenActuador({0,true,ORIGEN_MANUAL,1,"pump"}).exito);
    agua=1000; sensorValido=false;
    assert(!ejecutarOrdenActuador({0,true,ORIGEN_MANUAL,1,"pump"}).exito);
    sensorValido=true;
    assert(ejecutarOrdenActuador({1,true,ORIGEN_IR,1,"ir allowed"}).exito);
    assert(ejecutarOrdenActuador({1,false,ORIGEN_IR,1,"ir allowed"}).exito);
    assert(ejecutarOrdenActuador({0,true,ORIGEN_MANUAL,1,"pump"}).exito);
    reloj+=TIEMPO_MAXIMO_BOMBA_MS; verificarLimiteBomba(); assert(!estadoSalidas[0]);
    assert(propietarioSalidas[0]==PROPIETARIO_MANUAL_OFF);
    entrarModoSeguro("test");
    assert(!ejecutarOrdenActuador({1,true,ORIGEN_MANUAL,1,"on"}).exito);
    watchdogActivo=false; assert(!recuperarModoSeguro());
    watchdogActivo=true; heap=100; assert(!recuperarModoSeguro());
    heap=100000; assert(recuperarModoSeguro()); assert(!estadoSalidas[1]);
  }
  assert(!ejecutarOrdenActuador({-1,true,ORIGEN_MANUAL,1,"invalid"}).exito);
  assert(!ejecutarOrdenActuador({5,true,ORIGEN_MANUAL,1,"invalid"}).exito);
}
