/*
 * PROJECT DOMUS — esqueleto perfil final.
 * Compila el firmware con DRV8833 para motores, 74HC595 para luces,
 * y DFPlayer Mini para audio (perfil CASA_FINAL_DRV8833_DFPLAYER).
 */
#define DOMUS_PERFIL_CASA 4
#define DOMUS_SALIDAS_ECONOMICAS 0
#define MICROSD_HABILITADA false

// Arduino solo genera prototipos para el .ino principal, no para un .ino
// incluido. Demo feria IR+luces: solo la lectura de suelo se usa en loop.
bool leerHumedad(int &crudoSalida, int &pctSalida);

// Funciones de hardware del perfil final
void actualizarLuces74HC595(bool salaEncendida, bool cuartoEncendida, bool cultivoEncendida);

#include "../casa_inteligente_v4/casa_inteligente_v4.ino"
