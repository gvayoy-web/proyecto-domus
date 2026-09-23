/*
 * PROJECT DOMUS — esqueleto perfil final.
 * Compila el firmware con DRV8833 para motores, 74HC595 para luces,
 * y DFPlayer Mini para audio (perfil CASA_FINAL_DRV8833_DFPLAYER).
 */
#define DOMUS_PERFIL_CASA 4
#define DOMUS_SALIDAS_ECONOMICAS 0
#define MICROSD_HABILITADA false

// Arduino solo genera prototipos para el .ino principal, no para un .ino
// incluido. Estas son las tres funciones que la pantalla usa antes de su
// definicion dentro del firmware de producto.
bool leerHumedad(int &crudoSalida, int &pctSalida);
bool leerLuz(int &crudoSalida, int &pctSalida);
bool leerAmbiente(float &tempCSalida, float &humAireSalida);

// Funciones de hardware del perfil final
void actualizarLuces74HC595(bool salaEncendida, bool cuartoEncendida, bool cultivoEncendida);

#include "../casa_inteligente_v4/casa_inteligente_v4.ino"
